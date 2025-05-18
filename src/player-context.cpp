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

#include "player-context.h"
#include "checklist.h"
#include <iostream>

void PlayerContext::handleInfo(const EngineEvent& event) {
    if (!event.searchInfo.has_value()) return;
    const auto& searchInfo = *event.searchInfo;

    currentMove_.updateFromSearchInfo(searchInfo);

    if (searchInfo.currMove) {
        const auto move = gameState_.stringToMove(*searchInfo.currMove, requireLan_);
        Checklist::logCheck("currmove check", !move.isEmpty(),
            "Encountered illegal move " + *searchInfo.currMove + " in currMove, raw info line " + event.rawLine);
    }

    if (!searchInfo.pv.empty()) {
        std::vector<QaplaBasics::Move> pvMoves;
        pvMoves.reserve(searchInfo.pv.size());

        bool valid = true;
        for (const auto& moveStr : searchInfo.pv) {
            const auto move = gameState_.stringToMove(moveStr, requireLan_);
            if (move.isEmpty()) {
                std::string fullPv;
                for (const auto& m : searchInfo.pv)
                    fullPv += m + " ";
                if (!fullPv.empty()) fullPv.pop_back();
                Checklist::logCheck("PV check", true,
                    "Encountered illegal move " + moveStr + " in pv " + fullPv);
                valid = false;
                break;
            }
            gameState_.doMove(move);
            pvMoves.push_back(move);
        }

        for (size_t i = 0; i < pvMoves.size(); ++i)
            gameState_.undoMove();

        if (valid)
            Checklist::logCheck("PV check", true);
    }
}

void PlayerContext::handleBestMove(const EngineEvent& event) {
    if (!Checklist::logCheck("Computing a move returns a legal move", event.bestMove.has_value())) return;
    const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
    if (!Checklist::logCheck("Computing a move returns a legal move", !move.isEmpty(),
        "Encountered illegal move " + *event.bestMove + " in currMove, raw info line " + event.rawLine)) {
        gameState_.setGameResult(GameEndCause::IllegalMove, gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
        return;
    }
    checkTime(event);
    gameState_.doMove(move);
    currentMove_.updateFromBestMove(event, computeMoveStartTimestamp_);
}

void PlayerContext::checkTime(const EngineEvent& event) {
    const int64_t GRACE_MS = 5;
    const int64_t GRACE_NODES = 1000;

    const GoLimits limits = goLimits_;
    const bool white = gameState_.isWhiteToMove();
    const int64_t moveElapsedMs = event.timestampMs - computeMoveStartTimestamp_;

    const int64_t timeLeft = white ? limits.wtimeMs : limits.btimeMs;
    int numLimits = (timeLeft > 0) + limits.movetimeMs.has_value() +
        limits.depth.has_value() + limits.nodes.has_value();

    if (timeLeft > 0) {
        if (!Checklist::logCheck("wtime/btime overrun check", moveElapsedMs <= timeLeft,
            std::to_string(moveElapsedMs) + " > " + std::to_string(timeLeft))) {
            gameState_.setGameResult(GameEndCause::Timeout, white ? GameResult::BlackWins : GameResult::WhiteWins);
        }
    }

    if (limits.movetimeMs.has_value()) {
        Checklist::logCheck("movetime overrun check", moveElapsedMs < *limits.movetimeMs + GRACE_MS,
            std::to_string(moveElapsedMs) + " < " + std::to_string(*limits.movetimeMs));
        if (numLimits == 1) {
            Checklist::logCheck("movetime underrun check", moveElapsedMs > *limits.movetimeMs * 99 / 100,
                "The engine should use EXACTLY " + std::to_string(*limits.movetimeMs) +
                " ms but took " + std::to_string(moveElapsedMs));
        }
    }

    if (!event.searchInfo.has_value()) return;

    if (Checklist::logCheck("Engine provides search depth info", event.searchInfo->depth.has_value())) {
        if (limits.depth.has_value()) {
            int depth = *event.searchInfo->depth;
            Checklist::logCheck("depth overrun check", depth <= *limits.depth,
                std::to_string(depth) + " > " + std::to_string(*limits.depth));
            if (numLimits == 1) {
                Checklist::logCheck("depth underrun check", depth >= *limits.depth,
                    std::to_string(depth) + " > " + std::to_string(*limits.depth));
            }
        }
    }

    if (Checklist::logCheck("Engine provides nodes info", event.searchInfo->nodes.has_value())) {
        if (limits.nodes.has_value()) {
            int64_t nodes = *event.searchInfo->nodes;
            Checklist::logCheck("nodes overrun check", nodes <= *limits.nodes + GRACE_NODES,
                std::to_string(nodes) + " > " + std::to_string(*limits.nodes));
            if (numLimits == 1) {
                Checklist::logCheck("nodes underrun check", nodes > *limits.nodes * 9 / 10,
                    std::to_string(nodes) + " > " + std::to_string(*limits.nodes));
            }
        }
    }
}

void PlayerContext::setMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    if (!move.isEmpty()) {
        gameState_.doMove(move);
    }
}

bool PlayerContext::isLegalMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    return !move.isEmpty();
}
