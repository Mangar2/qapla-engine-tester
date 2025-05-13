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

#include "move-record.h"
#include "time-control.h"

enum class Side { White, Black };

/**
 * Stores a list of moves and manages current game state pointer.
 * Supports forward/backward navigation and time control evaluation.
 */
class GameRecord {
public:
    /** Adds a move at the current ply position, overwriting any future moves. */
    void addMove(const MoveRecord& move);

    /** Returns the current ply index. */
    size_t currentPly() const;

    /** Sets the current ply (0 = before first move). */
    void setPly(size_t ply);

    /** Advances to the next ply if possible. */
    void advance();

    /** Rewinds to the previous ply if possible. */
    void rewind();

    /** Computes total time used by a player up to current ply. */
    uint64_t timeUsed(Side side) const;

    /** Returns const reference to move history. */
    const std::vector<MoveRecord>& history() const;

private:
    std::vector<MoveRecord> moves_;
    size_t currentPly_ = 0;
};