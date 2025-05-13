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

#include "game-record.h"

void GameRecord::addMove(const MoveRecord& move) {
    if (currentPly_ < moves_.size()) {
        moves_.resize(currentPly_);
    }
    moves_.push_back(move);
    ++currentPly_;
}

size_t GameRecord::currentPly() const {
    return currentPly_;
}

void GameRecord::setPly(size_t ply) {
    if (ply <= moves_.size()) {
        currentPly_ = ply;
    }
}

void GameRecord::advance() {
    if (currentPly_ < moves_.size()) {
        ++currentPly_;
    }
}

void GameRecord::rewind() {
    if (currentPly_ > 0) {
        --currentPly_;
    }
}

uint64_t GameRecord::timeUsed(Side side) const {
    uint64_t total = 0;
    for (size_t i = 0; i < currentPly_; ++i) {
        if (static_cast<Side>(i % 2) == side) {
            total += moves_[i].timeMs;
        }
    }
    return total;
}

const std::vector<MoveRecord>& GameRecord::history() const {
    return moves_;
}
