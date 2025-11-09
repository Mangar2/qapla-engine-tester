/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 */

// GameRecord.h
#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <string>
#include <map>
#include <iostream>

#include "move-record.h"
#include "time-control.h"
#include "game-result.h"
#include "change-tracker.h"

#include "../qapla-engine/move.h"

namespace QaplaTester {

struct GameStruct
{
	std::string fen;
	std::string lanMoves;
	std::string sanMoves;
	// For winboard the move the engine has sent in original format;
	std::string originalMove;
	bool isWhiteToMove;
};

/**
 * @brief Event type for a game (used for tournament, EPD, SPRT, etc.)
 */
enum class GameEvent : std::uint8_t {
	None,
	Tournament,
	Epd,
	Sprt,
	ComputeTask
};

/**
 * Stores a list of moves and manages current game state pointer.
 * Supports forward/backward navigation and time control evaluation.
 */
class GameRecord
{
public:
	/**
	 * @brief Sets the starting position of the game.
	 * @param startPos True if starting from standard position, false for custom FEN.
	 * @param startFen The starting FEN string if not standard.
	 * @param isWhiteToMove True if white to move at start.
	 * @param startHalfmoves The halfmove clock at start.
	 */
	void setStartPosition(bool startPos, const std::string& startFen, bool isWhiteToMove, uint32_t startHalfmoves);
	/**
	 * @brief Sets the starting position of the game with engine names.
	 * @param startPos True if starting from standard position, false for custom FEN.
	 * @param startFen The starting FEN string if not standard.
	 * @param isWhiteToMove True if white to move at start.
	 * @param startHalfmoves The halfmove clock at start.
	 * @param whiteEngineName Name of the white engine.
	 * @param blackEngineName Name of the black engine.
	 */
	void setStartPosition(bool startPos, const std::string& startFen, bool isWhiteToMove, uint32_t startHalfmoves,
						  const std::string& whiteEngineName, const std::string& blackEngineName);

	/**
	 * @brief Initializes this GameRecord using another GameRecord (for PGN-based start setup).
	 * @param source Source GameRecord to extract setup information from.
	 * @param toPly Number of plies to import from the source game history.
	 * @param whiteEngineName White engine name to override.
	 * @param blackEngineName Black engine name to override.
	 */
	void setStartPosition(const GameRecord &source, uint32_t toPly,
						  const std::string &whiteEngineName, const std::string &blackEngineName);

	/** 
	 * @brief Sets the starting position in FEN format.
	 * Only sets the FEN string, does not alter the move list or any other state.
	 * @param fen The starting position as a FEN string.
	 */
	void setFen(const std::string &fen) {
		changeTracker_.trackModification();
		startFen_ = fen;
		startPos_ = false;
	}

	/**
	 * @brief Adds a move at the current ply position, overwriting any future moves.
	 * @param move The move record to add.
	 */
	void addMove(const MoveRecord &move);

	/** 
	 * @brief Updates the move at the current ply position. Does not change currentPly_. 
	 * Note: the chess move itself (move.move) must not be changed.
	 * 
	 * @param move The move to update.
	 * @return True if the update was successful, false otherwise.
	 */
	bool updateMove(const MoveRecord &move);

	/**
	 * @brief Returns the current ply index.
	 * @return Index of the next move (0 = before first move)
	 */
	[[nodiscard]] uint32_t nextMoveIndex() const;

	/**
	 * @brief Sets the current ply (0 = before first move).
	 * @param ply The ply index to set.
	 */
	void setNextMoveIndex(uint32_t ply);

	/**
	 * @brief Advances to the next ply if possible.
	 */
	void advance();

	/**
	 * @brief Rewinds to the previous ply if possible.
	 */
	void rewind();

	/**
	 * @brief Returns the total time used by each side up to the current ply.
	 * @return A pair of milliseconds used: {whiteTime, blackTime}
	 */
	[[nodiscard]] std::pair<uint64_t, uint64_t> timeUsed() const;

	/**
	 * @brief Returns const reference to move history.
	 * @return Reference to move history vector
	 */
	[[nodiscard]] const std::vector<MoveRecord> &history() const
	{
		return moves_;
	}

	/**
	 * @brief Returns non-const reference to move history.
	 * @return Reference to move history vector
	 */
	std::vector<MoveRecord> &history()
	{
		return moves_;
	}

	/**
	 * @brief Returns the index of the move record for the given halfmove number.
	 * @param halfmoveNo The halfmove number (1-based).
	 * @return std::optional<uint32_t> The index of the move record, or std::nullopt if not found.
	 */
	[[nodiscard]] std::optional<uint32_t> getHalfmoveIndex(uint32_t halfmoveNo) const {
		if (halfmoveNo <= startHalfmoves_) { return std::nullopt; }
		auto index = halfmoveNo - startHalfmoves_ - 1;
		if (index < moves_.size()) {
			return index;
		}
		return std::nullopt;
	}

	/**
	 * @brief Returns true if the game started with the standard starting position.
	 * @return True if standard position, false otherwise.
	 */
	[[nodiscard]] bool getStartPos() const { return startPos_; }

	/**
	 * @brief Returns the starting position in FEN format.
	 * @return The starting position as a FEN string.
	 */
	[[nodiscard]] std::string getStartFen() const { return startFen_; }

	/**
	 * @brief Sets the game end cause and result.
	 * Also updates the last move (if any) to reflect checkmate in SAN notation
	 * and stores the end cause and result in the move record.
	 * @param cause The cause of the game end.
	 * @param result The result of the game.
	 */
	void setGameEnd(GameEndCause cause, GameResult result);

	/**
	 * @brief Returns the game end cause and result.
	 * @return A pair of GameEndCause and GameResult.
	 */
	[[nodiscard]] std::tuple<GameEndCause, GameResult> getGameResult() const
	{
		return {gameEndCause_, gameResult_};
	}

	/**
	 * @brief Returns true if the game has ended (not unterminated).
	 * @return True if the game is over, false otherwise.
	 */
	[[nodiscard]] bool isGameOver() const
	{
		return gameResult_ != GameResult::Unterminated && currentPly_ == moves_.size();
	}

	/**
	 * @brief Sets the time control for the game.
	 * @param whiteTimeControl Time control for white.
	 * @param blackTimeControl Time control for black.
	 */
	void setTimeControl(const TimeControl &whiteTimeControl, const TimeControl &blackTimeControl)
	{
		changeTracker_.trackModification();
		whiteTimeControl_ = whiteTimeControl;
		blackTimeControl_ = blackTimeControl;
	}

	/**
	 * @brief Returns the white side's time control.
	 * @return Const reference to white time control.
	 */
	[[nodiscard]] const TimeControl &getWhiteTimeControl() const
	{
		return whiteTimeControl_;
	}

	/**
	 * @brief Returns non-const reference to white time control.
	 * @return Reference to white time control.
	 */
	TimeControl &getWhiteTimeControl()
	{
		return whiteTimeControl_;
	}

	/**
	 * @brief Returns the black side's time control.
	 * @return Const reference to black time control.
	 */
	[[nodiscard]] const TimeControl &getBlackTimeControl() const
	{
		return blackTimeControl_;
	}

	/**
	 * @brief Returns non-const reference to black time control.
	 * @return Reference to black time control.
	 */
	TimeControl &getBlackTimeControl()
	{
		return blackTimeControl_;
	}

	/**
	 * @brief Returns the current side to move.
	 * @return True if white to move, false if black.
	 */
	[[nodiscard]] bool isWhiteToMove() const
	{
		return wtmAtPly(currentPly_);
	}

	/**
	 * @brief Determines who was to move at the start of the game.
	 * @param ply The ply number.
	 * @return True if white was to move at ply 0, false if black.
	 */
	[[nodiscard]] bool wtmAtPly(size_t ply) const
	{
		return ply % 2 == 0 ? isWhiteToMoveAtStart_ : !isWhiteToMoveAtStart_;
	}

	/**
	 * @brief Returns the halfmove number at a specific ply.
	 * @param ply The ply number to check.
	 * @return uint32_t The halfmove number at the specified ply.
	 */
	[[nodiscard]] uint32_t halfmoveNoAtPly(size_t ply) const
	{
		return startHalfmoves_ + ply + 1;
	}

	/**
	 * @brief Gets the name of the white engine.
	 * @return Reference to white engine name string
	 */
	[[nodiscard]] const std::string &getWhiteEngineName() const
	{
		return whiteEngineName_;
	}

	/**
	 * @brief Sets the name of the white engine.
	 * @param name The engine name.
	 */
	void setWhiteEngineName(const std::string &name)
	{
		changeTracker_.trackModification();
		whiteEngineName_ = name;
	}

	/**
	 * @brief Gets the name of the black engine.
	 * @return Reference to black engine name string
	 */
	[[nodiscard]] const std::string &getBlackEngineName() const
	{
		return blackEngineName_;
	}

	/**
	 * @brief Sets the name of the black engine.
	 * @param name The engine name.
	 */
	void setBlackEngineName(const std::string &name)
	{
		changeTracker_.trackModification();
		blackEngineName_ = name;
	}

	/**
	 * @brief Sets the round number of the game.
	 * @param round The round number to set.
	 * @param gameInRound The game number within the round.
	 * @param opening The number of opening selected.
	 */
	void setTournamentInfo(uint32_t round, uint32_t gameInRound, uint32_t opening)
	{
		changeTracker_.trackModification();
		round_ = round;
		gameInRound_ = gameInRound;
		opening_ = opening;
	}

	/**
	 * @brief Gets the round number of the game.
	 * @return The round number.
	 */
	[[nodiscard]] uint32_t getRound() const
	{
		return round_;
	}

	/**
	 * @brief Gets the opening number used as start position for the game.
	 * @return The opening number.
	 */
	[[nodiscard]] uint32_t getOpeningNo() const
	{
		return opening_;
	}

	/**
	 * @brief Sets the name of the starting position (e.g. ECO code, opening name or epd name)
	 * @param positionName The name of the starting position.
	 */
	void setPositionName(const std::string &positionName)
	{
		changeTracker_.trackModification();
		positionName_ = positionName;
	}

	/**
	 * @brief Gets the name of the starting position (e.g. ECO code, opening name or epd name)
	 * @return The name of the starting position.
	 */
	[[nodiscard]] const std::string &getPositionName() const
	{
		return positionName_;
	}

	/**
	 * @brief Gets the game number of the current round
	 * @return The game number within the current round.
	 */
	[[nodiscard]] uint32_t getGameInRound() const
	{
		return gameInRound_;
	}

	/**
	 * @brief Sets the game numbver of the current round
	 * @param gameInRound The game number within the current round.
	 */
	void setGameInRound(uint32_t gameInRound)
	{
		gameInRound_ = gameInRound;
	}

	/**
	 * @brief Sets the total game number from the start of the tournament.
	 *
	 * This method assigns a unique game number that represents the position of the
	 * current game in the overall sequence of all games played in the tournament,
	 * across all rounds. For example, in a tournament with 10 rounds and 10 games
	 * per round, the 5th game in the 5th round would have a total game number of 45.
	 *
	 * @param totalGameNo The total game number to set.
	 */
	void setTotalGameNo(uint32_t totalGameNo)
	{
		totalGameNo_ = totalGameNo;
	}

	/**
	 * @brief Gets the total game number
	 * @return The 1-indexed number of total games played in the tournament so far
	 */
	[[nodiscard]] uint32_t getTotalGameNo() const
	{
		return totalGameNo_;
	}

	/**
	 * @brief Sets a PGN tag key-value pair.
	 * @param key The tag name.
	 * @param value The tag value.
	 */
	void setTag(const std::string &key, const std::string &value)
	{
		changeTracker_.trackModification();
		if (value.empty())
		{
			tags_.erase(key);
		}
		else
		{
			tags_[key] = value;
		}
	}

	/**
	 * @brief Gets the value of a PGN tag by key.
	 * @param key The tag name.
	 * @return Tag value or empty string if not found.
	 */
	[[nodiscard]] std::string getTag(const std::string &key) const
	{
		auto it = tags_.find(key);
		return it != tags_.end() ? it->second : "";
	}
	/**
	 * @brief Gets all stored PGN tags.
	 * @return Map of all tag key-value pairs.
	 */
	[[nodiscard]] const std::map<std::string, std::string> &getTags() const
	{
		return tags_;
	}

	/**
	 * @brief Checks if this game record is an update from another.
	 * @param other The other game record to compare with.
	 * @return True if the records differ, false if they are the same.
	 */
	[[nodiscard]] bool isUpdate(const GameRecord &other) const;

	/**
	 * @brief Checks if this game record is different from another.
	 * @param other The other game record to compare with.
	 * @return True if the records differ, false if they are the same.
	 */
	[[nodiscard]] bool isDifferent(const GameRecord &other) const;

	/**
	 * @brief Creates a GameStruct containing the essential game data.
	 *
	 * This method generates a `GameStruct` object that includes the starting FEN
	 * position and a concatenated list of moves in both LAN (long algebraic notation)
	 * and SAN (short algebraic notation).
	 *
	 * @return A `GameStruct` object
	 */
	[[nodiscard]] GameStruct createGameStruct() const;

	/**
	 * @brief Render all moves up to and including the given ply index into a single PGN-compatible string.
	 * The numbering is based on halfmove numbers (uses halfmoveNoAtPly) and the provided MoveRecord::toString options
	 * are forwarded for move annotations.
	 * Note: it will not include any PGN tags or result indication especially no start fen.
	 *
	 * @param lastPly inclusive ply index (0 = first ply)
	 * @param opts formatting options forwarded to MoveRecord::toString
	 * @return concatenated move string (SANs with optional comments)
	 */
	[[nodiscard]] std::string movesToStringUpToPly(uint32_t lastPly, const MoveRecord::toStringOptions& opts = {}) const;

	/**
	 * @brief Reserves memory for the move history to avoid repeated reallocations.
	 *
	 * This method pre-allocates memory for the internal move history (`moves_`),
	 * ensuring that no additional memory allocations are required when adding
	 * the specified number of moves. This is useful when the number of moves
	 * to be added is known in advance, improving performance.
	 *
	 * @param count The number of moves to reserve space for.
	 */
	void reserveMoves(size_t count)
	{
		moves_.reserve(count);
	}

	/**
	 * @brief Creates a minimal copy of this GameRecord.
	 *
	 * This method generates a new `GameRecord` object that contains only the essential
	 * information from the current record. The minimal copy includes:
	 * - Start position (FEN and side to move)
	 * - Move history with reduced data (excluding original and SAN moves, comments, NAGs, search info)
	 * - Engine names
	 * - Game end cause and result
	 * - PGN tags
	 * - Tournament information (round, game number, opening)
	 *
	 * Fields that are excluded in the minimal copy:
	 * - Original and SAN moves in each `MoveRecord`
	 * - Comments and NAGs in each `MoveRecord`
	 * - Search info list and update count in each `MoveRecord`
	 *
	 * @return A new `GameRecord` object with reduced data.
	 */
	[[nodiscard]] GameRecord createMinimalCopy() const;


	/**
	 * @brief Get the Change Tracker object
	 * 
	 * @return const ChangeTracker& 
	 */
	[[nodiscard]] const ChangeTracker& getChangeTracker() const { return changeTracker_; }

private:
	std::map<std::string, std::string> tags_;
	bool startPos_ = true;
	std::string startFen_;
	std::string positionName_; ///> Optional name of the starting position (e.g. ECO code, opening name or epd name)
	std::vector<MoveRecord> moves_;
	uint32_t currentPly_ = 0;
	GameEndCause gameEndCause_ = GameEndCause::Ongoing;
	GameResult gameResult_ = GameResult::Unterminated;
	TimeControl whiteTimeControl_;
	TimeControl blackTimeControl_;
	std::string whiteEngineName_;
	std::string blackEngineName_;
	bool isWhiteToMoveAtStart_ = true;
	uint32_t startHalfmoves_ = 0;
	uint32_t totalGameNo_ = 0;
	uint32_t gameInRound_ = 0;
	uint32_t opening_ = 0;
	uint32_t round_ = 0;
	ChangeTracker changeTracker_;
};

} // namespace QaplaTester
