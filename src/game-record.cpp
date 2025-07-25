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


#include "game-record.h"

void GameRecord::setStartPosition(bool startPos, std::string startFen, bool isWhiteToMove,
    std::string whiteEngineName, std::string blackEngineName) {
    moves_.clear();
    isWhiteToMove_ = isWhiteToMove;
    currentPly_ = 0;
    startPos_ = startPos;
    startFen_ = startPos ? "" : startFen;
    gameEndCause_ = GameEndCause::Ongoing;
    gameResult_ = GameResult::Unterminated;
    whiteEngineName_ = whiteEngineName;
    blackEngineName_ = blackEngineName;
}

void GameRecord::setStartPosition(const GameRecord& source, uint32_t toPly, bool isWhiteToMove,
    const std::string& whiteEngineName, const std::string& blackEngineName) {
    moves_.clear();
    const auto& sourceHistory = source.history();
    moves_.insert(moves_.end(), sourceHistory.begin(), sourceHistory.begin() + 
        std::min<uint32_t>(toPly, static_cast<uint32_t>(sourceHistory.size())));
    isWhiteToMove_ = isWhiteToMove;
    currentPly_ = 0;
    startPos_ = source.startPos_;
    startFen_ = source.startFen_;
    gameEndCause_ = GameEndCause::Ongoing;
    gameResult_ = GameResult::Unterminated;
    whiteEngineName_ = whiteEngineName;
    blackEngineName_ = blackEngineName;
    round_ = source.round_;
    tags_ = source.tags_;
}

void GameRecord::addMove(const MoveRecord& move) {
    if (currentPly_ < moves_.size()) {
        moves_.resize(currentPly_);
    }
    moves_.push_back(move);
    ++currentPly_;
	isWhiteToMove_ = !isWhiteToMove_;
}

uint32_t GameRecord::nextMoveIndex() const {
    return currentPly_;
}

void GameRecord::setNextMoveIndex(uint32_t ply) {
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

std::pair<uint64_t, uint64_t> GameRecord::timeUsed() const {
    uint64_t whiteTime = 0;
    uint64_t blackTime = 0;

    for (size_t i = 0; i < currentPly_ && i < moves_.size(); ++i) {
        if (i % 2 == 0) {
            whiteTime += moves_[i].timeMs;
        }
        else {
            blackTime += moves_[i].timeMs;
        }
    }

    return { whiteTime, blackTime };
}


