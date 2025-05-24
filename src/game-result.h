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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#pragma once

#include <string>

/**
 * @brief Enumerates all meaningful game termination types.
 *
 * Includes PGN-standard outcomes and additional technical results relevant
 * for engine testing or protocol-level termination.
 */
enum class GameEndCause {
	Ongoing,               // The game is still in progress
	Checkmate,             // One player is checkmated
	Stalemate,             // The game ended in stalemate
	DrawByRepetition,      // Draw due to threefold repetition
	DrawByFiftyMoveRule,   // Draw due to the 50-move rule
	DrawByInsufficientMaterial, // Draw due to insufficient mating material
	DrawByAgreement,       // Draw by mutual agreement (PGN result: ���)
	Resignation,           // One side resigns
	Timeout,               // One side ran out of time
	IllegalMove,           // A player made an illegal move (e.g. engine bug)
	Adjudication,          // Tester or supervisor declared a result externally
	Forfeit,               // Forfeit due to rule violation or technical fault
	TerminatedByTester,    // Game was aborted or terminated by the test system
	Disconnected		   // Game was aborted due to engine not responding
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
	case GameEndCause::Disconnected: return "disconnected";
	default: return "unknown";
	}
}
inline std::string to_string(GameEndCause cause) { return gameEndCauseToPgnTermination(cause); }

inline std::string gameResultToPgnResult(GameResult result) {
	switch (result) {
		case GameResult::WhiteWins: return "1-0"; break;
		case GameResult::BlackWins: return "0-1"; break;
		case GameResult::Draw:      return "1/2-1/2"; break;
		default:                    return "*"; break;
	}
}

