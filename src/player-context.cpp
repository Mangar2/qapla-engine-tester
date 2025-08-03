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
#include "engine-report.h"
#include "timer.h"
#include "engine-worker-factory.h"
#include "app-error.h"

void PlayerContext::checkPV(const EngineEvent& event) {
    if (!event.searchInfo) return;
    const auto& searchInfo = *event.searchInfo;

    if (searchInfo.pv.empty()) return;

    auto& state = computeState_ == ComputeState::ComputingMove ? gameState_ : ponderState_;
    std::vector<QaplaBasics::Move> pvMoves;
    pvMoves.reserve(searchInfo.pv.size());

    for (const auto& moveStr : searchInfo.pv) {
        const auto move = state.stringToMove(moveStr, requireLan_);
        if (move.isEmpty()) {
            std::string fullPv;
            for (const auto& m : searchInfo.pv)
                fullPv += m + " ";
            if (!fullPv.empty()) fullPv.pop_back();
            std::string stateStr = toString(computeState_);
            checklist_->logReport("pv", false,
                "Encountered illegal move " + moveStr + " while " + stateStr + " in pv " + fullPv);
            
            Logger::engineLogger().log(engine_->getIdentifier() + " Illegal move in PV: " + moveStr + " while " + stateStr +
                " in raw info line \"" + event.rawLine + "\"", TraceLevel::info);
            return;
        }
        state.doMove(move);
        pvMoves.push_back(move);
    }

    for (size_t i = 0; i < pvMoves.size(); ++i)
        state.undoMove();
}


void PlayerContext::handleInfo(const EngineEvent& event) {
    if (!event.searchInfo.has_value()) return;
    const auto& searchInfo = *event.searchInfo;

    currentMove_.updateFromSearchInfo(searchInfo);

    if (searchInfo.currMove) {
        auto& state = computeState_ == ComputeState::ComputingMove ? gameState_ : ponderState_;
        const auto move = state.stringToMove(*searchInfo.currMove, requireLan_);
        checklist_->logReport("currmove", !move.isEmpty(),
            "Encountered illegal move " + *searchInfo.currMove + " in currMove, raw info line \"" + event.rawLine + "\"");
        if (move.isEmpty()) {
            Logger::engineLogger().log(engine_->getIdentifier() + " Illegal move in currMove: " + *searchInfo.currMove +
                " in raw info line \"" + event.rawLine + "\"", TraceLevel::info);
        }
	}

    checkPV(event);

    if (searchInfo.depth)            checklist_->report("depth", true);
    if (searchInfo.selDepth)         checklist_->report("seldepth", true);
    if (searchInfo.multipv)          checklist_->report("multipv", true);
    if (searchInfo.scoreCp)          checklist_->report("score cp", true);
    if (searchInfo.scoreMate)        checklist_->report("score mate", true);
    if (searchInfo.timeMs)           checklist_->report("time", true);
    if (searchInfo.nodes)            checklist_->report("nodes", true);
    if (searchInfo.nps)              checklist_->report("nps", true);
    if (searchInfo.hashFull)         checklist_->report("hashfull", true);
    if (searchInfo.cpuload)          checklist_->report("cpuload", true);
    if (searchInfo.currMoveNumber)   checklist_->report("currmovenumber", true);

}

QaplaBasics::Move PlayerContext::handleBestMove(const EngineEvent& event) {
    if (computeState_ != ComputeState::ComputingMove) {
        Logger::engineLogger().log(engine_->getIdentifier() + "Received best move while not computing a move, ignoring.", 
            TraceLevel::error);
        return QaplaBasics::Move();
    }
    computeState_ = ComputeState::Idle;
    if (!checklist_->logReport("legalmove", event.bestMove.has_value())) {
        gameState_.setGameResult(GameEndCause::IllegalMove, 
            gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
		currentMove_ = MoveRecord{};
        return QaplaBasics::Move();
    }
    const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
    if (!checklist_->logReport("legalmove", !move.isEmpty(),
        "Encountered illegal move \"" + *event.bestMove + "\" in bestmove, raw info line \"" + event.rawLine + "\"")) {
        gameState_.setGameResult(GameEndCause::IllegalMove, 
            gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
        currentMove_ = MoveRecord{};
        Logger::engineLogger().log(engine_->getIdentifier() + " Illegal move in bestmove: " + *event.bestMove +
            " in raw info line \"" + event.rawLine + "\"", TraceLevel::info);
        return QaplaBasics::Move();
    }
    checkTime(event);
    gameState_.doMove(move);

    currentMove_.updateFromBestMove(event, move.getLAN(), gameState_.moveToSan(move),
        computeMoveStartTimestamp_, gameState_.getHalfmoveClock());
    return move;
}

void PlayerContext::checkTime(const EngineEvent& event) {
    const uint64_t GRACE_MS = 100;
    const uint64_t GRACE_NODES = 1000;
        
    const bool white = gameState_.isWhiteToMove();
    const uint64_t moveElapsedMs = event.timestampMs - computeMoveStartTimestamp_;

    const uint64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    int numLimits = goLimits_.hasTimeControl + goLimits_.movetimeMs.has_value() +
        goLimits_.depth.has_value() + goLimits_.nodes.has_value();

    if (goLimits_.hasTimeControl) {
		timeControl_.toPgnTimeControlString();
        if (!checklist_->logReport("no-loss-on-time", moveElapsedMs <= timeLeft,
            "Timecontrol: " + timeControl_.toPgnTimeControlString() + " Used time: " + 
            std::to_string(moveElapsedMs) + " ms. Available Time: " + std::to_string(timeLeft) + " ms")) {
            gameState_.setGameResult(GameEndCause::Timeout, white ? GameResult::BlackWins : GameResult::WhiteWins);
        }
    }

    if (goLimits_.movetimeMs.has_value()) {
        checklist_->logReport("no-move-time-overrun", moveElapsedMs < *goLimits_.movetimeMs + GRACE_MS,
            "took " + std::to_string(moveElapsedMs) + " ms, limit is " + std::to_string(*goLimits_.movetimeMs) + " ms", 
            TraceLevel::warning);
        if (numLimits == 1 && EngineReport::reportUnderruns) {
            checklist_->logReport("no-move-time-underrun", moveElapsedMs > *goLimits_.movetimeMs * 99 / 100,
                "The engine should use EXACTLY " + std::to_string(*goLimits_.movetimeMs) +
                " ms but took " + std::to_string(moveElapsedMs), 
                TraceLevel::info);
        }
    }

    if (!event.searchInfo.has_value()) return;

    if (checklist_->logReport("depth", event.searchInfo->depth.has_value())) {
        if (goLimits_.depth.has_value()) {
            uint32_t depth = *event.searchInfo->depth;
            checklist_->logReport("no-depth-overrun", depth <= *goLimits_.depth,
                std::to_string(depth) + " > " + std::to_string(*goLimits_.depth));
            if (numLimits == 1) {
                checklist_->logReport("no-depth-underrun", depth >= *goLimits_.depth,
                    std::to_string(depth) + " > " + std::to_string(*goLimits_.depth));
            }
        }
    }

    if (checklist_->logReport("nodes", event.searchInfo->nodes.has_value())) {
        if (goLimits_.nodes.has_value()) {
            uint64_t nodes = *event.searchInfo->nodes;
            checklist_->logReport("no-nodes-overrun", nodes <= *goLimits_.nodes + GRACE_NODES,
                std::to_string(nodes) + " > " + std::to_string(*goLimits_.nodes));
            if (numLimits == 1) {
                checklist_->logReport("no-nodes-underrun", nodes > *goLimits_.nodes * 9 / 10,
                    std::to_string(nodes) + " > " + std::to_string(*goLimits_.nodes));
            }
        }
    }
}

bool PlayerContext::checkEngineTimeout() {
    if (computeState_ != ComputeState::ComputingMove) return false;
    if (!engine_) return false;
	const uint64_t GRACE_MS = 1000;
    const uint64_t OVERRUN_TIMEOUT = 5000;

    uint64_t moveElapsedMs = Timer::getCurrentTimeMs() - computeMoveStartTimestamp_;
    moveElapsedMs = moveElapsedMs < GRACE_MS ? 0 : moveElapsedMs - GRACE_MS;

    const bool white = gameState_.isWhiteToMove();
    bool restarted = false;

    const uint64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    uint64_t overrun = 0;

	if (goLimits_.hasTimeControl) {
        overrun = moveElapsedMs > timeLeft + OVERRUN_TIMEOUT;
        if (moveElapsedMs > timeLeft) {
			engine_->moveNow();
			restarted = restartIfNotReady();
            gameState_.setGameResult(restarted ? GameEndCause::Disconnected : GameEndCause::Timeout, 
                white ? GameResult::BlackWins : GameResult::WhiteWins);
            if (!restarted) {
                checklist_->logReport("no-loss-on-time", restarted, "Engine timeout and not reacting for a while, but answered isready");
            }
            Logger::engineLogger().log(engine_->getIdentifier() + " Engine timeout or disconnect", 
                TraceLevel::warning);
		}
	}
    else if ((goLimits_.movetimeMs.has_value() && *goLimits_.movetimeMs < moveElapsedMs)) {
        overrun = moveElapsedMs > *goLimits_.movetimeMs + OVERRUN_TIMEOUT;
        engine_->moveNow();
        restarted = restartIfNotReady();
    }
	if (overrun && !restarted) {
        // We are here, if the engine responded with isready but still does not play a move
        restartEngine();
        restarted = true;
	}
    if (restarted) {
        checklist_->logReport("no-disconnect", !restarted, "Engine timeout and not reacting to isready, restarted ");
    }
    return restarted;
}

void PlayerContext::handleDisconnect(bool isWhitePlayer) {
    gameState_.setGameResult(GameEndCause::Disconnected, isWhitePlayer ? GameResult::BlackWins : GameResult::WhiteWins);
    checklist_->logReport("no-disconnect", false, "Engine disconnected unexpectedly.");
    restartEngine();
}

void PlayerContext::restartEngine() {
	if (!engine_) {
		throw AppError::make("PlayerContext::restart; Cannot restart without an engine.");
	}
    if (!isEventQueueThread) {
		std::cerr << "PlayerContext::restartEngine called outside of the GameManager thread. This is not allowed." << std::endl;
        throw AppError::make("PlayerContext::restart; Cannot restart engine outside of the GameManager thread.");
	}
    computeState_ = ComputeState::Idle;
    // Create a fully initialized new engine instance (incl. UCI handshake)
    engine_ = EngineWorkerFactory::restart(*engine_);
}

bool PlayerContext::restartIfNotReady() {
    std::chrono::seconds WAIT_READY{ 1 };
	if (engine_ && !engine_->requestReady(WAIT_READY)) {
        restartEngine();
		return true;
	}
    return false;
}

void PlayerContext::doMove(QaplaBasics::Move move) {
	if (move.isEmpty()) {
		throw AppError::make("PlayerContext::doMove; Illegal move in for doMove");
	}
	if (!engine_) {
		throw AppError::make("PlayerContext::doMove; Cannot do move without an engine.");
	}
    // This method is only called with a checked move thus beeing empty should never happen
    std::string lanMove = move.getLAN();
    if (computeState_ == ComputeState::Pondering && !ponderMove_.empty()) {
        computeState_ = ponderMove_ == lanMove ? ComputeState::PonderHit : ComputeState::PonderMiss;
    }
    ponderMove_ = "";  

    if (computeState_ == ComputeState::PonderMiss) {
        // moveNow with option true will wait until bestmove received and consider the bestmove as
        // handshake. The bestmove is then not send to the GameManager
		auto success = engine_->moveNow(true);
        const auto& id = engine_->getIdentifier();
        if (!checklist_->logReport("correct-pondering", success,
            "stop command to engine " + id + " did not return a bestmove while in pondermode in time")) {
			Logger::engineLogger().log(id + " Stop on ponder-miss did not return a bestmove in time", TraceLevel::error);
			// Try to heal the situation by requesting a ready state from the engine
            engine_->requestReady();
        }
    }
    gameState_.doMove(move);
}

void PlayerContext::computeMove(const GameRecord& gameRecord, const GoLimits& goLimits) {
	if (!engine_) {
		throw AppError::make("PlayerContext::computeMove; Cannot compute move without an engine.");
	}
	if (computeState_ == ComputeState::ComputingMove) {
		throw AppError::make("PlayerContext::computeMove; Cannot compute move while already computing a move.");
	}

    currentMove_.clear();
    goLimits_ = goLimits;
    // Race-condition safety setting. We will get the true timestamp returned from the EngineProcess sending
    // the compute move string to the engine. As it is asynchronous, we might get a bestmove event before receiving the
    // sent compute move event. In this case we use this timestamp here
    setComputeMoveStartTimestamp(Timer::getCurrentTimeMs());
    // Do not set computeState_ to ComputeMove true, as computeMove is asynchronous.
    // Instead, rely on the SendingComputeMove marker event to ensure correct temporal ordering
    // in the GameManager's event queue. This avoids misclassifying late-arriving pondering info
    // as part of the new compute phase.
    engine_->computeMove(gameRecord, goLimits, computeState_ == ComputeState::PonderHit);
}

void PlayerContext::allowPonder(const GameRecord& gameRecord, const GoLimits& goLimits, 
    const std::optional<EngineEvent>& event) {
	if (!engine_) {
		throw AppError::make("PlayerContext::allowPonder; Cannot allow pondering without an engine.");
	}
    if (!engine_->getConfig().isPonderEnabled()) return;
    if (!event) return;

	if (event->type != EngineEvent::Type::BestMove) {
		throw AppError::make("PlayerContext::allowPonder; Best move event required to ponder.");
	}
    if (computeState_ == ComputeState::ComputingMove) {
		throw AppError::make("PlayerContext::allowPonder; Cannot allow pondering while already computing a move.");
	}
	goLimits_ = goLimits;
    currentMove_.clear();
    ponderMove_ = event->ponderMove ? *event->ponderMove : "";

    if (!ponderMove_.empty()) {
        const auto move = gameState_.stringToMove(ponderMove_, requireLan_);
        if (checklist_->logReport("legal-pondermove", !move.isEmpty(),
            "Encountered illegal ponder move \"" + ponderMove_ + "\" in currMove, raw info line \"" + event->rawLine + "\"")) {
            ponderState_.synchronizeIncrementalFrom(gameState_);
            ponderState_.doMove(move);
			auto [cause, result] = ponderState_.getGameResult();
			if (result != GameResult::Unterminated) {
				// If the game is already over, we cannot ponder
				ponderMove_.clear();
                ponderState_.undoMove();
			} 
            else {
                computeState_ = ComputeState::Pondering;
                engine_->allowPonder(gameRecord, goLimits, ponderMove_);
            }
        }
    }
    else {
        computeState_ = ComputeState::Pondering;
		engine_->allowPonder(gameRecord, goLimits, ponderMove_);
    }

}

