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

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../qapla-engine/movegenerator.h"
#include "game-result.h"
#include "game-record.h"

namespace QaplaTester {

using MoveStr = std::string;
using MoveStrList = std::vector<MoveStr>;

 /**
  * @brief Represents the current state of a chess game for engine interaction,
  *        including starting setup and played moves.
  */
class GameState {
public:
	GameState();

	/**
	 * @brief Checks if it's White's turn to move.
	 * @return True if it's White's turn, false otherwise.
	 */
	[[nodiscard]] bool isWhiteToMove() const { return position_.isWhiteToMove(); }

	/**
	 * @brief Get the current position in FEN notation.
	 * @return The FEN string representing the current position.
	 */
	[[nodiscard]] std::string getFen() const { return position_.getFen(); }

	/**
	 * @brief Get the number of half moves without pawn move or capture from the starting position (from FEN).
	 * 
	 * This is the halfmove clock value from the FEN string of the starting position.
	 * It does not include any moves played in the current game.
	 * 
	 * @return The halfmove clock value from the starting position FEN.
	 */
	[[nodiscard]] uint32_t getStartHalfmoves() const { return position_.getStartHalfmoves(); }

	/**
	 * @brief Get the Fullmove Number according to FEN specification.
	 * 
	 * The fullmove number starts at 1 and is incremented after Black
	 * 's move. It is derived from the halfmoves of the fen + played halfmoves.
	 * 
	 * @return The fullmove number.
	 */
	[[nodiscard]] uint32_t getFullmoveNumber() const { 
		return getHalfmovesPlayed() / 2 + 1;
	}

	/**
	 * @brief Sets the fullmove number for the starting position (from FEN).
	 * This resets the current halfmove counter to zero.
	 * 
	 * @param fullmoves The fullmove number from the starting position FEN.
	 */
	void setSetupFullmoveNumber(uint32_t fullmoves) {
		if (fullmoves == 0) {
			fullmoves = 1;
		}
		uint32_t halfmoves = (fullmoves - 1) * 2;
		if (!position_.isWhiteToMove()) {
			halfmoves += 1;
		}
		auto currentHalfmoves = static_cast<uint32_t>(moveList_.size());
		halfmoves = std::max(halfmoves, currentHalfmoves);
		position_.setStartHalfmoves(halfmoves - currentHalfmoves);
	}

	/**
	 * @brief Performs a move on the current position and updates the move list.
	 * @param move The move to perform.
	 */
	void doMove(const QaplaBasics::Move& move);

	/**
	 * @brief Sets the game position to a specific FEN string.
	 * @param startPos If true, sets the position to the starting position.
	 * @param fen The FEN string to set, if startPos = false. 
	 * @return True if the FEN string was valid and the position was set, false otherwise.
	 */
	bool setFen(bool startPos, const std::string& fen = "");

	/**
	 * @brief Undo the last move and restore the previous position.
	 */
	void undoMove();

	/**
	 * Find the correct move providing a partial move information
	 * @param moveStr The move string to parse.
	 */
	QaplaBasics::Move stringToMove(const std::string& moveStr, bool requireLan);

	/**
	 * Attempts to resolve a move from partially specified parameters.
	 *
	 * @param movingPiece Moving piece type if known.
	 * @param fromSquare Source square if specified.
	 * @param toSquare Destination square if specified.
	 * @param promotionPiece Promotion piece type if applicable.
	 * @return Tuple of:
	 *         - Move if uniquely identified, otherwise empty.
	 *         - Bool indicating that one or more moves match.
	 *         - Bool indicating that all matching moves are promotions.
	 */
	std::tuple<QaplaBasics::Move, bool, bool> resolveMove(
		std::optional<QaplaBasics::Piece> movingPiece,
		std::optional<QaplaBasics::Square> fromSquare,
		std::optional<QaplaBasics::Square> toSquare,
		std::optional<QaplaBasics::Piece> promotionPiece);

	/**
	 * @brief Returns a move as San notation. The move must be a legal move in the current position.
	 * @param move The move to convert to San notation.
	 */
	[[nodiscard]] std::string moveToSan(const QaplaBasics::Move& move) const;

	/**
	 * @brief Checks if the game is over and returns the result.
	 * @return The result of the game and the winner side.
	 */
	std::tuple<GameEndCause, GameResult> getGameResult();

	/**
	 * @brief Get the Halfmove Clock -> the total number of half moves without pawn move or capture
	 * ("total includes the start value from fen to implement the 50-moves-draw rule")
	 * 
	 * @return the total number of half moves without pawn move or capture
	 */
	[[nodiscard]] uint32_t getHalfmoveClock() const {
		return position_.getTotalHalfmovesWithoutPawnMoveOrCapture();
	}

	/**
	 * @brief Sets the halfmove clock for the starting position (from FEN).
	 * This resets the current halfmove counter to zero.
	 * 
	 * @param halfmoves The halfmove clock value from the starting position FEN.
	 */
	void setSetupHalfmoveClock(uint32_t halfmoves) {
		position_.setFenHalfmovesWihtoutPawnMoveOrCapture(halfmoves);
		position_.setHalfmovesWithoutPawnMoveOrCapture(0);
	}

	/**
	 * @brief Get the number of half moves played so far in the game.
	 * This includes the starting position's halfmoves.
	 * 
	 * @return The total number of half moves played.
	 */ 
	[[nodiscard]]uint32_t getHalfmovesPlayed() const {
		return position_.getStartHalfmoves() + static_cast<uint32_t>(moveList_.size());
	}

	/**
	 * @brief Sets the game result and the cause of the game end.
	 * @param cause The cause of the game end.
	 * @param result The result of the game.
	 */
	void setGameResult(GameEndCause cause, GameResult result) {
		gameEndCause_ = cause;
		gameResult_ = result;
	}

	/**
	 * @brief sets the game state from a game record information and copies the GameRecord until the given ply number.
	 * @param game The game record to set the state from.
	 * @param plies The ply number to set the game state to.
	 * @param verbose If true, loggs warnings if the game record is invalid.
	 * @return The copy of the GameRecord up to the given ply number.
	 */
	GameRecord setFromGameRecordAndCopy(const GameRecord& game, 
		std::optional<uint32_t> plies = std::nullopt, bool verbose = true);

	/**
	 * @brief Sets the game state from a game record information.
	 * 
	 * This method sets the game state to the position described by the given GameRecord.
	 * It does not create and return a copy of the GameRecord and is thus faster than setFromGameRecordAndCopy.
	 * 
	 * @param game The game record to set the state from.
	 * @param plies The ply number to set the game state to. If not provided, the full game record is used.
	 */
	void setFromGameRecord(const GameRecord& game, std::optional<uint32_t> plies = std::nullopt);

	/**
	 * @brief Incrementally synchronizes this GameState to match the move history of the given reference state.
	 *
	 * This method assumes that both GameStates were originally in sync and have diverged only by a small number
	 * of recent moves. It avoids full history or FEN comparison for performance reasons. It performs:
	 * 1. A single undo if the last move differs from the reference at the same position.
	 * 2. A reapplication of all moves present in the reference but missing here.
	 *
	 * This is not a perfect synchronization; correctness depends on both states sharing a common history.
	 *
	 * @param referenceState The GameState whose move history this state should approximate.
	 */
	void synchronizeIncrementalFrom(const GameState& referenceState);
	
	/**
	 * @brief Gets the board position after all moves played so far.
	 * 
	 * @return The current board position.
	 */
	[[nodiscard]] const QaplaMoveGenerator::MoveGenerator& position() const {
		return position_;
	}
	QaplaMoveGenerator::MoveGenerator& position() {
		return position_;
	}
	
private:
	/**
	 * @brief Computes if the game is over and returns the result based on the chess board.
	 * @return The result of the game and the winner side.
	 */
	std::tuple<GameEndCause, GameResult> computeGameResult();
    QaplaMoveGenerator::MoveGenerator position_;

	[[nodiscard]] bool isThreefoldRepetition() const;

	QaplaBasics::MoveList legalMoves_; // legalMoves of the current position
	bool moveListOutdated = true;
	std::vector<QaplaBasics::Move> moveList_;  // list of moves played so far
	std::vector<QaplaBasics::BoardState> boardState_; // list of board states
	std::vector<uint64_t> hashList_; // list of hash values
	GameEndCause gameEndCause_ = GameEndCause::Ongoing; // cause of game end
	GameResult gameResult_ = GameResult::Unterminated; // result of the game
};

} // namespace QaplaTester
