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
#include "movegenerator.h"
#include "game-result.h"
#include "game-record.h"

using MoveStr = std::string;
using MoveStrList = std::vector<MoveStr>;

 /**
  * @brief Represents the current state of a chess game for engine interaction,
  *        including starting setup and played moves.
  */
class GameState {
public:
	GameState();

	bool isWhiteToMove() const { return position_.isWhiteToMove(); }

	std::string getFen() const { return position_.getFen(); }

	/**
	 * @brief Performs a move on the current position and updates the move list.
	 * @param move The move to perform.
	 */
	void doMove(const QaplaBasics::Move& move);

	/**
	 * @brief Sets the game position to a specific FEN string.
	 * @param startPos If true, sets the position to the starting position.
	 * @param fen The FEN string to set, if startPos = false. 
	 */
	void setFen(bool startPos, const std::string fen = "");

	/**
	 * @brief Undo the last move and restore the previous position.
	 */
	void undoMove();

	/**
	 * Find the correct move providing a partial move information
	 */
	QaplaBasics::Move stringToMove(std::string move, bool requireLan);

	/**
	 * @brief Returns a move as San notation. The move must be a legal move in the current position.
	 * @param move The move to convert to San notation.
	 */
	std::string moveToSan(const QaplaBasics::Move& move) const;

	/**
	 * @brief Checks if the game is over and returns the result.
	 * @return The result of the game and the winner side.
	 */
	std::tuple<GameEndCause, GameResult> getGameResult();

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
	 * @brief sets the game state from a game record information
	 * @param record The game record to set the state from.
	 * @param plies The ply number to set the game state to.
	 */
	GameRecord setFromGameRecord(const GameRecord& record, std::optional<int> plies = std::nullopt);

	
private:
	/**
	 * @brief Computes if the game is over and returns the result based on the chess board.
	 * @return The result of the game and the winner side.
	 */
	std::tuple<GameEndCause, GameResult> computeGameResult();
    QaplaMoveGenerator::MoveGenerator position_;

	bool isThreefoldRepetition() const;

	MoveList legalMoves_; // legalMoves of the current position
	bool moveListOutdated = true;
	std::vector<QaplaBasics::Move> moveList_;  // list of moves played so far
	std::vector<QaplaBasics::BoardState> boardState_; // list of board states
	std::vector<uint64_t> hashList_; // list of hash values
	GameEndCause gameEndCause_ = GameEndCause::Ongoing; // cause of game end
	GameResult gameResult_ = GameResult::Unterminated; // result of the game
};
