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
#include "qapla-engine/move.h"

namespace QaplaTester {

void GameRecord::setStartPosition(bool startPos, const std::string& startFen, bool isWhiteToMove, uint32_t startHalfmoves)
{
    moves_.clear();
    isWhiteToMoveAtStart_ = isWhiteToMove;
    currentPly_ = 0;
    startHalfmoves_ = startHalfmoves;
    startPos_ = startPos;
    startFen_ = startPos ? "" : startFen;
    gameEndCause_ = GameEndCause::Ongoing;
    gameResult_ = GameResult::Unterminated;
    changeTracker_.trackModification();
}

void GameRecord::setStartPosition(bool startPos, const std::string& startFen, bool isWhiteToMove, uint32_t startHalfmoves,
                                  const std::string& whiteEngineName, const std::string& blackEngineName)
{
    setStartPosition(startPos, startFen, isWhiteToMove, startHalfmoves);
    whiteEngineName_ = whiteEngineName;
    blackEngineName_ = blackEngineName;
}

void GameRecord::setStartPosition(const GameRecord &source, uint32_t toPly,
                                  const std::string &whiteEngineName, const std::string &blackEngineName)
{
    moves_.clear();
    const auto &sourceHistory = source.history();
    moves_.insert(moves_.end(), sourceHistory.begin(), sourceHistory.begin() + std::min<uint32_t>(toPly, static_cast<uint32_t>(sourceHistory.size())));
    isWhiteToMoveAtStart_ = source.isWhiteToMoveAtStart_;
    currentPly_ = 0;
    startPos_ = source.startPos_;
    startFen_ = source.startFen_;
    gameEndCause_ = GameEndCause::Ongoing;
    gameResult_ = GameResult::Unterminated;
    whiteEngineName_ = whiteEngineName;
    blackEngineName_ = blackEngineName;
    round_ = source.round_;
    tags_ = source.tags_;
    changeTracker_.trackModification();
}

void GameRecord::addMove(const MoveRecord &move)
{
    if (currentPly_ < moves_.size())
    {
        moves_.resize(currentPly_);
        gameEndCause_ = GameEndCause::Ongoing;
        gameResult_ = GameResult::Unterminated;
        changeTracker_.trackModification();
    }
    moves_.push_back(move);
    ++currentPly_;
    changeTracker_.trackUpdate();
}

bool GameRecord::updateMove(const MoveRecord &move)
{
    if (currentPly_ == 0 || currentPly_ > moves_.size())
    {
        return false;
    }
    if (moves_[currentPly_ - 1].move != move.move)
    {
        return false;
    }
    moves_[currentPly_ - 1] = move;
    changeTracker_.trackModification();
    return true;
}

void GameRecord::setGameEnd(GameEndCause cause, GameResult result)
{
    changeTracker_.trackModification();
    gameEndCause_ = cause;
    gameResult_ = result;

    // Update the last move if there is one
    if (currentPly_ > 0 && currentPly_ <= moves_.size())
    {
        auto& lastMove = moves_[currentPly_ - 1];
        lastMove.endCause_ = cause;
        lastMove.result_ = result;

        // If this is checkmate, change '+' to '#' in SAN notation
        if (cause == GameEndCause::Checkmate && !lastMove.san_.empty())
        {
            if (lastMove.san_.back() == '+')
            {
                lastMove.san_.back() = '#';
            }
            else if (lastMove.san_.find('+') != std::string::npos)
            {
                // Handle cases like "Qxf7+" or similar
                size_t pos = lastMove.san_.find('+');
                lastMove.san_[pos] = '#';
            }
        }
    }
}

uint32_t GameRecord::nextMoveIndex() const
{
    return currentPly_;
}

void GameRecord::setNextMoveIndex(uint32_t ply)
{
    if (ply <= moves_.size())
    {
        changeTracker_.trackModification();
        currentPly_ = ply;
    }
}

void GameRecord::advance()
{
    if (currentPly_ < moves_.size())
    {
        changeTracker_.trackUpdate();
        ++currentPly_;
    }
}

void GameRecord::rewind()
{
    if (currentPly_ > 0)
    {
        changeTracker_.trackUpdate();
        --currentPly_;
    }
}

std::pair<uint64_t, uint64_t> GameRecord::timeUsed() const
{
    uint64_t whiteTime = 0;
    uint64_t blackTime = 0;

    for (size_t i = 0; i < currentPly_ && i < moves_.size(); ++i)
    {
        if (i % 2 == 0)
        {
            whiteTime += moves_[i].timeMs;
        }
        else
        {
            blackTime += moves_[i].timeMs;
        }
    }

    return {whiteTime, blackTime};
}

bool GameRecord::isUpdate(const GameRecord &other) const
{
    return other.changeTracker_.checkModification(changeTracker_).second;
}

bool GameRecord::isDifferent(const GameRecord &other) const
{
    return startPos_ != other.startPos_ ||
           startFen_ != other.startFen_ ||
           isWhiteToMoveAtStart_ != other.isWhiteToMoveAtStart_ ||
           whiteEngineName_ != other.whiteEngineName_ ||
           blackEngineName_ != other.blackEngineName_ ||
           round_ != other.round_ ||
           tags_ != other.tags_ ||
           moves_.size() != other.moves_.size() ||
           currentPly_ != other.currentPly_ ||
           whiteTimeControl_ != other.whiteTimeControl_ ||
           blackTimeControl_ != other.blackTimeControl_ ||
           gameEndCause_ != other.gameEndCause_ ||
           gameResult_ != other.gameResult_;
}

GameStruct GameRecord::createGameStruct() const
{
    GameStruct gs;
    gs.fen = getStartFen();

    size_t moveCount = moves_.size();
    gs.lanMoves.reserve(moveCount * 6);
    gs.sanMoves.reserve(moveCount * 5);

    std::string spacer{};
    uint32_t index = 0;
    for (const auto &move : moves_)
    {
        if (index >= currentPly_) {
            break;
        }
        index++;

        gs.lanMoves += spacer + move.lan_;
        gs.sanMoves += spacer + move.san_;
        spacer = " ";
    }
    gs.isWhiteToMove = isWhiteToMove();
    gs.originalMove = currentPly_ == 0 ? "" : moves_[currentPly_ - 1].original;

    return gs;
}

GameRecord GameRecord::createMinimalCopy() const
{
    GameRecord record;
    record.startPos_ = startPos_;
    record.startFen_ = startFen_;
    record.currentPly_ = currentPly_;
    record.gameEndCause_ = gameEndCause_;
    record.gameResult_ = gameResult_;
    record.whiteEngineName_ = whiteEngineName_;
    record.blackEngineName_ = blackEngineName_;
    record.isWhiteToMoveAtStart_ = isWhiteToMoveAtStart_;
    record.round_ = round_;
    record.gameInRound_ = gameInRound_;
    record.totalGameNo_ = totalGameNo_;
    record.opening_ = opening_;
    record.moves_.reserve(moves_.size());
    for (const auto &move : moves_)
    {
        record.moves_.emplace_back(move.createMinimalCopy());
    }

    return record;
}

std::string GameRecord::movesToStringUpToPly(uint32_t lastPly, const MoveRecord::toStringOptions& opts) const {

    constexpr size_t MAX_LINE_LENGTH = 80;
    std::ostringstream out;
    if (moves_.empty()) {
        return "";
    }

    uint32_t maxIndex = std::min<uint32_t>(lastPly, static_cast<uint32_t>(moves_.size() - 1));

    // If the game started with Black to move, PGN may print an initial "N... " prefix
    // where N is the fullmove number of the first printed halfmove when that halfmove is Black.
    // Compute the absolute halfmove number of the first printed ply and decide.
    uint32_t firstHalfmove = halfmoveNoAtPly(0);
    bool wtm = isWhiteToMoveAtStart_;
    uint32_t moveNumber = (firstHalfmove + 1) / 2;
    
    std::string currentLine;
    if (!wtm) {
        currentLine = std::to_string(moveNumber) + "...";
    }

    for (uint32_t i = 0; i <= maxIndex; ++i) {
        std::ostringstream moveStream;
        
        // Build the complete move string (number + move + comment)
        if (wtm) {
            moveStream << moveNumber << ". ";
            moveNumber++;
        }
        moveStream << moves_[i].toString(opts);
        
        std::string moveStr = moveStream.str();
        
        // Check if adding this move would exceed 80 characters
        // Account for space separator between moves
        size_t spaceNeeded = currentLine.empty() ? 0 : 1;
        size_t neededLength = currentLine.length() + spaceNeeded + moveStr.length();
        
        if (!currentLine.empty() && neededLength > MAX_LINE_LENGTH) {
            // Write current line and start a new one
            out << currentLine << "\n";
            currentLine = moveStr;
        } else {
            // Add to current line
            if (!currentLine.empty()) {
                currentLine += " ";
            }
            currentLine += moveStr;
        }
        
        wtm = !wtm;
    }
    
    // Write any remaining content
    if (!currentLine.empty()) {
        out << currentLine;
    }

    return out.str();
}

} // namespace QaplaTester
