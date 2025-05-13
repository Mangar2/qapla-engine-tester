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

using MoveStr = std::string;
using MoveStrList = std::vector<MoveStr>;

/**
 * @brief Enumerates all meaningful game termination types.
 *
 * Includes PGN-standard outcomes and additional technical results relevant
 * for engine testing or protocol-level termination.
 */
enum class GameEndCause {
	Ongoing,               ///< The game is still in progress
	Checkmate,             ///< One player is checkmated
	Stalemate,             ///< The game ended in stalemate
	DrawByRepetition,      ///< Draw due to threefold repetition
	DrawByFiftyMoveRule,   ///< Draw due to the 50-move rule
	DrawByInsufficientMaterial, ///< Draw due to insufficient mating material
	DrawByAgreement,       ///< Draw by mutual agreement (PGN result: ½–½)
	Resignation,           ///< One side resigns
	Timeout,               ///< One side ran out of time
	IllegalMove,           ///< A player made an illegal move (e.g. engine bug)
	Adjudication,          ///< Tester or supervisor declared a result externally
	Forfeit,               ///< Forfeit due to rule violation or technical fault
	TerminatedByTester     ///< Game was aborted or terminated by the test system
};

enum class GameResult { WhiteWins, BlackWins, Draw, Unterminated };

inline std::string gameEndCauseToPgnTermination(GameEndCause cause) {
	switch (cause) {
	case GameEndCause::Checkmate: return "checkmate";
	case GameEndCause::Stalemate: return "stalemate";
	case GameEndCause::DrawByRepetition: return "threefold repetition";
	case GameEndCause::DrawByFiftyMoveRule: return "50-move rule";
	case GameEndCause::DrawByInsufficientMaterial: return "insufficient material";
	case GameEndCause::DrawByAgreement: return "draw agreement";
	case GameEndCause::Resignation: return "resignation";
	case GameEndCause::Timeout: return "time forfeit";
	case GameEndCause::IllegalMove: return "illegal move";
	case GameEndCause::Adjudication: return "adjudication";
	case GameEndCause::Forfeit: return "forfeit";
	case GameEndCause::TerminatedByTester: return "terminated";
	default: return "unknown";
	}
}

inline std::string gameResultToPgnResult(GameResult result) {
	switch (result) {
		case GameResult::WhiteWins: return "1-0"; break;
		case GameResult::BlackWins: return "0-1"; break;
		case GameResult::Draw:      return "1/2-1/2"; break;
		default:                    return "*"; break;
	}
}


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
	 * @brief Checks if the game is over and returns the result.
	 * @return The result of the game and the winner side.
	 */
	std::tuple<GameEndCause, GameResult> getGameResult();
	
private:
    QaplaMoveGenerator::MoveGenerator position_;

	bool isThreefoldRepetition() const;

	std::vector<QaplaBasics::Move> moveList_;  // list of moves played so far
	std::vector<QaplaBasics::BoardState> boardState_; // list of board states
	std::vector<uint64_t> hashList_; // list of hash values
};
