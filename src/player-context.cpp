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

void PlayerContext::handleInfo(const EngineEvent& event) {
    if (!event.searchInfo.has_value()) return;
    const auto& searchInfo = *event.searchInfo;

    currentMove_.updateFromSearchInfo(searchInfo);

    if (searchInfo.currMove) {
        const auto move = gameState_.stringToMove(*searchInfo.currMove, requireLan_);
        checklist_->logReport("currmove", !move.isEmpty(),
            "Encountered illegal move " + *searchInfo.currMove + " in currMove, raw info line \"" + event.rawLine + "\"");
	}

    if (!searchInfo.pv.empty()) {
        std::vector<QaplaBasics::Move> pvMoves;
        pvMoves.reserve(searchInfo.pv.size());
        for (const auto& moveStr : searchInfo.pv) {
            const auto move = gameState_.stringToMove(moveStr, requireLan_);
            if (move.isEmpty()) {
                std::string fullPv;
                for (const auto& m : searchInfo.pv)
                    fullPv += m + " ";
                if (!fullPv.empty()) fullPv.pop_back();
				std::string stateStr = computingMove_ ? "computing move" : 
					pondering_ ? "pondering" : "inactive";
                checklist_->logReport("pv", false,
                    "Encountered illegal move " + moveStr + " while " + stateStr + " in pv " + fullPv);

                break;
            }
            gameState_.doMove(move);
            pvMoves.push_back(move);
        }

        for (size_t i = 0; i < pvMoves.size(); ++i)
            gameState_.undoMove();

    }

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
    computingMove_ = false;
    if (!checklist_->logReport("legalmove", event.bestMove.has_value())) {
        gameState_.setGameResult(GameEndCause::IllegalMove, 
            gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
		currentMove_ = MoveRecord{};
        return QaplaBasics::Move::EMPTY_MOVE;
    }
    const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
    if (!checklist_->logReport("legalmove", !move.isEmpty(),
        "Encountered illegal move \"" + *event.bestMove + "\" in currMove, raw info line \"" + event.rawLine + "\"")) {
        gameState_.setGameResult(GameEndCause::IllegalMove, 
            gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
        currentMove_ = MoveRecord{};
        return QaplaBasics::Move::EMPTY_MOVE;
    }
    checkTime(event);
    gameState_.doMove(move);
    currentMove_.updateFromBestMove(event, computeMoveStartTimestamp_);
    return move;
}

void PlayerContext::checkTime(const EngineEvent& event) {
    const int64_t GRACE_MS = 100;
    const int64_t GRACE_NODES = 1000;

    
    const bool white = gameState_.isWhiteToMove();
    const int64_t moveElapsedMs = event.timestampMs - computeMoveStartTimestamp_;

    const int64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    int numLimits = (timeLeft > 0) + goLimits_.movetimeMs.has_value() +
        goLimits_.depth.has_value() + goLimits_.nodes.has_value();

    if (timeLeft > 0) {
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
            int depth = *event.searchInfo->depth;
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
            int64_t nodes = *event.searchInfo->nodes;
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
    if (!computingMove_) return false;
    if (!engine_) return false;
	const int64_t GRACE_MS = 1000;
    const int64_t OVERRUN_TIMEOUT = 5000;

    const int64_t moveElapsedMs = Timer::getCurrentTimeMs() - computeMoveStartTimestamp_ - GRACE_MS;
    const bool white = gameState_.isWhiteToMove();
    bool restarted = false;

    const int64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    int64_t overrun = 0;
	if (timeLeft != 0) {
        overrun = moveElapsedMs > timeLeft + OVERRUN_TIMEOUT;
        if (moveElapsedMs > timeLeft) {
			engine_->moveNow();
			restarted = restartIfNotReady();
            gameState_.setGameResult(GameEndCause::Disconnected, white ? GameResult::BlackWins : GameResult::WhiteWins);
		}
	}
    else if ((goLimits_.movetimeMs.has_value() && *goLimits_.movetimeMs < moveElapsedMs)) {
        overrun = moveElapsedMs > *goLimits_.movetimeMs + OVERRUN_TIMEOUT;
        engine_->moveNow();
        restarted = restartIfNotReady();

    }
	if (overrun) {
        // We are here, if the engine responded with isready but still does not play a move
        restart();
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
    restart();
}

void PlayerContext::restart() {
	if (!engine_) {
		throw AppError::make("PlayerContext::restart; Cannot restart without an engine.");
	}
    computingMove_ = false;
    // Create a fully initialized new engine instance (incl. UCI handshake)
    engine_ = std::move(EngineWorkerFactory::restart(*engine_));
}

bool PlayerContext::restartIfNotReady() {
    std::chrono::seconds WAIT_READY{ 1 };
	if (engine_ && !engine_->requestReady(WAIT_READY)) {
        restart();
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
    std::string lan = move.getLAN();
	if (pondering_ && ponderMove_ == lan) {
		// We are in pondering mode and the move is the pondermove. We do not need to do or undo a move.
		// The game state is already correct.
		return;
	}
    if (pondering_ && ponderMove_ != "" && ponderMove_ != lan) {
        // Game state has the position after the pondermove. As the pondermove is not played
		// we undo the move to get back to the position before the pondermove.
        gameState_.undoMove();
        // moveNow with option true will wait until bestmove received and consider the bestmove as
        // handshake. The bestmove is then not send to the GameManager
		auto success = engine_->moveNow(true);
        auto id = engine_->getIdentifier();
        if (!checklist_->logReport("correct-pondering", success,
            "stop command to engine " + id + " did not return a bestmove while in pondermode in time")) {
			Logger::engineLogger().log(id + " Pondering did not return a bestmove in time", TraceLevel::error);
			// Try to heal the situation by requesting a ready state from the engine
            engine_->requestReady();
        }
    }
    pondering_ = false;
    ponderMove_ = "";
    gameState_.doMove(move);
}

bool PlayerContext::isLegalMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    return !move.isEmpty();
}

void PlayerContext::computeMove(const GameRecord& gameRecord, const GoLimits& goLimits) {
	if (!engine_) {
		throw AppError::make("PlayerContext::computeMove; Cannot compute move without an engine.");
	}
	if (computingMove_) {
		throw AppError::make("PlayerContext::computeMove; Cannot compute move while already computing a move.");
	}
	bool ponderHit = pondering_ && ponderMove_ != "";

    currentMove_.clear();
    goLimits_ = goLimits;
    // Race-condition safety setting. We will get the true timestamp returned from the EngineProcess sending
    // the compute move string to the engine. As it is asynchronous, we might get a bestmove event before receiving the
    // sent compute move event. In this case we use this timestamp here
    setComputeMoveStartTimestamp(Timer::getCurrentTimeMs());
    pondering_ = false;
	ponderMove_ = "";
    computingMove_ = true;
    engine_->computeMove(gameRecord, goLimits, ponderHit);
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
    if (computingMove_) {
		throw AppError::make("PlayerContext::allowPonder; Cannot allow pondering while already computing a move.");
	}
	goLimits_ = goLimits;
    currentMove_.clear();
    ponderMove_ = event->ponderMove ? *event->ponderMove : "";

    if (ponderMove_ != "") {
        const auto move = gameState_.stringToMove(ponderMove_, requireLan_);
        if (checklist_->logReport("legal-pondermove", !move.isEmpty(),
            "Encountered illegal ponder move \"" + ponderMove_ + "\" in currMove, raw info line \"" + event->rawLine + "\"")) {
            gameState_.doMove(move);
			auto [cause, result] = gameState_.getGameResult();
			if (result != GameResult::Unterminated) {
				// If the game is already over, we cannot ponder
				pondering_ = false;
				ponderMove_ = "";
                gameState_.undoMove();
			} 
            else {
                pondering_ = true;
                engine_->allowPonder(gameRecord, goLimits, ponderMove_);
            }
        }
    }
    else {
        pondering_ = true;
		engine_->allowPonder(gameRecord, goLimits, ponderMove_);
    }

}

