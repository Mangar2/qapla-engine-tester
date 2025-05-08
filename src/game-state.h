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
#include "game-start-position.h"  // enthält GameType + FEN

using Move = std::string;
using MoveList = std::vector<Move>;

 /**
  * @brief Represents the current state of a chess game for engine interaction,
  *        including starting setup and played moves.
  */
class GameState {
public:
    GameState() = default;

    const GameStartPosition& startPosition() const { return start_; }
    void setStartPosition(const GameStartPosition& pos) { start_ = pos; }

    const std::vector<std::string>& moveList() const { return moves_; }
    void addMove(std::string move) { moves_.push_back(std::move(move)); }
    void clearMoves() { moves_.clear(); }

    int fullmoveNumber() const { return fullmoveNumber_; }
    void setFullmoveNumber(int number) { fullmoveNumber_ = number; }

    bool whiteToMove() const { return whiteToMove_; }
    void setWhiteToMove(bool white) { whiteToMove_ = white; }

private:
    GameStartPosition start_;              // initial setup (variant + FEN)
    std::vector<std::string> moves_;       // move list in UCI format
    int fullmoveNumber_ = 1;               // starting move number
    bool whiteToMove_ = true;              // whose turn it is
};
