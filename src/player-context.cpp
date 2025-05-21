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

#include <iostream>
#include <chrono>
#include "player-context.h"
#include "checklist.h"
#include "timer.h"
#include "engine-worker-factory.h"

void PlayerContext::handleInfo(const EngineEvent& event) {
    if (!event.searchInfo.has_value()) return;
    const auto& searchInfo = *event.searchInfo;

    currentMove_.updateFromSearchInfo(searchInfo);

    if (searchInfo.currMove) {
        const auto move = gameState_.stringToMove(*searchInfo.currMove, requireLan_);
        Checklist::logCheck("Search info reports correct current move", !move.isEmpty(),
            "Encountered illegal move " + *searchInfo.currMove + " in currMove, raw info line \"" + event.rawLine + "\"");
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

    if (searchInfo.depth) Checklist::logCheck("Search info reports correct depth", true);
    if (searchInfo.selDepth) Checklist::logCheck("Search info reports correct selective depth", true);
    if (searchInfo.multipv) Checklist::logCheck("Search info reports correct multipv", true);
    if (searchInfo.scoreCp) Checklist::logCheck("Search info reports correct score", true);
    if (searchInfo.timeMs) Checklist::logCheck("Search info reports correct time", true);
    if (searchInfo.nodes) Checklist::logCheck("Search info reports correct nodes", true);
    if (searchInfo.nps) Checklist::logCheck("Search info reports correct nps", true);
    if (searchInfo.hashFull) Checklist::logCheck("Search info reports correct hashfull", true);
    if (searchInfo.cpuload) Checklist::logCheck("Search info reports correct cpuload", true);
    if (searchInfo.currMoveNumber) Checklist::logCheck("Search info reports correct move number", true);
}

QaplaBasics::Move PlayerContext::handleBestMove(const EngineEvent& event) {
    computingMove_ = false;
    if (!Checklist::logCheck("Computing a move returns a legal move", event.bestMove.has_value())) {
        gameState_.setGameResult(GameEndCause::IllegalMove, gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
		currentMove_ = MoveRecord{};
        return QaplaBasics::Move::EMPTY_MOVE;
    }
    const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
    if (!Checklist::logCheck("Computing a move returns a legal move", !move.isEmpty(),
        "Encountered illegal move \"" + *event.bestMove + "\" in currMove, raw info line \"" + event.rawLine + "\"")) {
        gameState_.setGameResult(GameEndCause::IllegalMove, gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
        currentMove_ = MoveRecord{};
        return QaplaBasics::Move::EMPTY_MOVE;
    }
    checkTime(event);
    gameState_.doMove(move);
    currentMove_.updateFromBestMove(event, computeMoveStartTimestamp_);
    return move;
}

void PlayerContext::checkTime(const EngineEvent& event) {
    const int64_t GRACE_MS = 5;
    const int64_t GRACE_NODES = 1000;

    
    const bool white = gameState_.isWhiteToMove();
    const int64_t moveElapsedMs = event.timestampMs - computeMoveStartTimestamp_;

    const int64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    int numLimits = (timeLeft > 0) + goLimits_.movetimeMs.has_value() +
        goLimits_.depth.has_value() + goLimits_.nodes.has_value();

    if (timeLeft > 0) {
        if (!Checklist::logCheck("No loss in time", moveElapsedMs <= timeLeft,
            std::to_string(moveElapsedMs) + " > " + std::to_string(timeLeft))) {
            gameState_.setGameResult(GameEndCause::Timeout, white ? GameResult::BlackWins : GameResult::WhiteWins);
        }
    }

    if (goLimits_.movetimeMs.has_value()) {
        Checklist::logCheck("No movetime overrun", moveElapsedMs < *goLimits_.movetimeMs + GRACE_MS,
            "took " + std::to_string(moveElapsedMs) + " ms, limit is " + std::to_string(*goLimits_.movetimeMs) + " ms");
        if (numLimits == 1) {
            Checklist::logCheck("No movetime underrun", moveElapsedMs > *goLimits_.movetimeMs * 99 / 100,
                "The engine should use EXACTLY " + std::to_string(*goLimits_.movetimeMs) +
                " ms but took " + std::to_string(moveElapsedMs));
        }
    }

    if (!event.searchInfo.has_value()) return;

    if (Checklist::logCheck("Engine provides search depth info", event.searchInfo->depth.has_value())) {
        if (goLimits_.depth.has_value()) {
            int depth = *event.searchInfo->depth;
            Checklist::logCheck("No depth overrun", depth <= *goLimits_.depth,
                std::to_string(depth) + " > " + std::to_string(*goLimits_.depth));
            if (numLimits == 1) {
                Checklist::logCheck("No depth underrun", depth >= *goLimits_.depth,
                    std::to_string(depth) + " > " + std::to_string(*goLimits_.depth));
            }
        }
    }

    if (Checklist::logCheck("Engine provides nodes info", event.searchInfo->nodes.has_value())) {
        if (goLimits_.nodes.has_value()) {
            int64_t nodes = *event.searchInfo->nodes;
            Checklist::logCheck("No nodes overrun", nodes <= *goLimits_.nodes + GRACE_NODES,
                std::to_string(nodes) + " > " + std::to_string(*goLimits_.nodes));
            if (numLimits == 1) {
                Checklist::logCheck("No nodes underrun", nodes > *goLimits_.nodes * 9 / 10,
                    std::to_string(nodes) + " > " + std::to_string(*goLimits_.nodes));
            }
        }
    }
}

bool PlayerContext::checkEngineTimeout() {
    if (!computingMove_) return false;
	const int64_t GRACE_MS = 2000;
    const int64_t OVERRUN_TIMEOUT = 5000;

    const int64_t moveElapsedMs = Timer::getCurrentTimeMs() - computeMoveStartTimestamp_ - GRACE_MS;
    const bool white = gameState_.isWhiteToMove();
    bool restarted = false;

    const int64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    int64_t overrun = 0;
	if (timeLeft != 0) {
        overrun = moveElapsedMs > timeLeft;
        if (moveElapsedMs > timeLeft) {
			engine_->moveNow();
			restarted = restartIfNotReady();
            gameState_.setGameResult(GameEndCause::Disconnected, white ? GameResult::BlackWins : GameResult::WhiteWins);
		}
	}
    else if ((goLimits_.movetimeMs.has_value() && *goLimits_.movetimeMs < moveElapsedMs)) {
        overrun = moveElapsedMs > *goLimits_.movetimeMs;
        engine_->moveNow();
        restarted = restartIfNotReady();

    }
	if (overrun) {
        // We are here, if the engine responded with isready but still does not play a move
        restart();
        restarted = true;
	}
    if (restarted) {
        Checklist::logCheck("No disconnect", restarted, "Engine not reacting to isready ");
    }
    return restarted;
}

void PlayerContext::restart() {
    auto list = EngineWorkerFactory::createUci(engine_->getExecutablePath(), std::nullopt, 1);
    engine_ = std::move(list[0]);
}

bool PlayerContext::restartIfNotReady() {
    std::chrono::seconds WAIT_READY{ 1 };
	if (engine_ && !engine_->requestReady(WAIT_READY)) {
        restart();
		return true;
	}
    return false;
}

void PlayerContext::doMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    if (!move.isEmpty()) {
        gameState_.doMove(move);
    }
}

bool PlayerContext::isLegalMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    return !move.isEmpty();
}

void PlayerContext::computeMove(const GameRecord& gameRecord, const GoLimits& goLimits) {
    goLimits_ = goLimits;
    computingMove_ = true;
    // Race-condition safety setting. We will get the true timestamp returned from the EngineProcess sending
    // the compute move string to the engine. As it is asynchronous, we might get a bestmove event before receiving the
    // sent compute move event. In this case we use this timestamp here
    setComputeMoveStartTimestamp(Timer::getCurrentTimeMs());
    engine_->computeMove(gameRecord, goLimits);
}

