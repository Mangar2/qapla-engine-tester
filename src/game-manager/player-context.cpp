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

#include "../engine-tester/engine-report.h"
#include "../engine-handling/engine-worker-factory.h"

#include "../base-elements/timer.h"
#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"

#include <format>
#include <iostream>
#include <chrono>

namespace QaplaTester {

using QaplaHelpers::Timer;

void PlayerContext::checkPV(const EngineEvent& event) {
    if (!event.searchInfo) { return; }
    const auto& searchInfo = *event.searchInfo;

    if (searchInfo.pv.empty()) { return; }

    std::scoped_lock lock(stateMutex_);
    // if we are pondering but we do not know the ponder move we lack information to check the ponder PV
    // Usually happening for winboard engines as they choose themselves what move they ponder on.
    if (computeState_ != ComputeState::ComputingMove && ponderMove_.empty()) {
        // Should not be necessary, see how engines behave maybe activate it again.
        // New Xboard documentation sais the engine should either send the full pv including ponder move
        // or a Hint: move that we use as ponder move.
        // return;
    }

    std::string invalidMove;

    if (isAssumedPondering()) {
        // If failed, try ponder state. We do not receive a handshake from xboard engines when pondering is stopped.
        invalidMove = validatePVAgainstState(ponderState_, searchInfo.pv);
    } else {
        invalidMove = validatePVAgainstState(gameState_, searchInfo.pv);
        
        // If failed, try ponder state. We do not receive a handshake from xboard engines when pondering is stopped.
        // Thus we might have race conditions. 
        if (!invalidMove.empty()) {
            invalidMove = validatePVAgainstState(ponderState_, searchInfo.pv);
        }
    }

    if (!invalidMove.empty()) {
        // Build full PV string for error reporting
        std::string fullPv;
        for (const auto& move : searchInfo.pv) {
            fullPv += move;
            fullPv += " ";
        }
        if (!fullPv.empty()) { 
            fullPv.pop_back(); 
        }
        std::string stateStr = toString(computeState_);
        logReport("pv", false,
            std::format("Encountered illegal move '{}' in pv while {}: {}", invalidMove, stateStr, fullPv));
        
        EngineLogger::engineLogger({.engineId = engine_->getIdentifier()}).
            log(std::format("{} Illegal move '{}' in PV while {} in raw info line \"{}\"", 
                engine_->getIdentifier(), invalidMove, stateStr, event.rawLine), TraceLevel::info);
    }
}

std::string PlayerContext::validatePVAgainstState(GameState& state, const std::vector<std::string>& pv) const {
    uint32_t pvCount = 0;
    for (const auto& moveStr : pv) {
        const auto move = state.stringToMove(moveStr, requireLan_);
        if (move.isEmpty()) {
            // Undo all moves we applied
            for (uint32_t i = 0; i < pvCount; ++i) {
                state.undoMove();
            }
            return moveStr;  // Return the invalid move
        }
        state.doMove(move);
        pvCount++;
    }

    // Undo all moves
    for (uint32_t i = 0; i < pvCount; ++i) {
        state.undoMove();
    }
    return "";  // Empty string means all moves valid
}


void PlayerContext::handleInfo(const EngineEvent& event) {
    if (!event.searchInfo.has_value()) {
        return;
    }
    const auto& searchInfo = *event.searchInfo;
    bool whitePovCorrection = !gameState_.isWhiteToMove() && engine_->getConfig().isScoreFromWhitePov();
    {
        std::scoped_lock lock(currentMoveMutex_);
        currentMove_.updateFromSearchInfo(searchInfo, whitePovCorrection);
    }

    uint64_t moveElapsedMs = Timer::getCurrentTimeMs() - computeMoveStartTimestamp_;
    currentMove_.timeMs = moveElapsedMs;

    if (searchInfo.currMove) {
        auto& state = computeState_ == ComputeState::ComputingMove ? gameState_ : ponderState_;
        const auto move = state.stringToMove(*searchInfo.currMove, requireLan_);
        logReport("currmove", !move.isEmpty(),
            std::format("Encountered illegal move {} in currMove, raw info line \"{}\"", *searchInfo.currMove, event.rawLine));
        if (move.isEmpty()) {
            EngineLogger::engineLogger({.engineId = engine_->getIdentifier()}).
            log(std::format("{} Illegal move in currMove: {} in raw info line \"{}\"", 
                engine_->getIdentifier(), *searchInfo.currMove, event.rawLine), TraceLevel::info);
        }
	}

    checkPV(event);

    if (searchInfo.depth)            { report("depth", true); }
    if (searchInfo.selDepth)         { report("seldepth", true); }
    if (searchInfo.multipv)          { report("multipv", true); }
    if (searchInfo.scoreCp)          { report("score cp", true); }
    if (searchInfo.scoreMate)        { report("score mate", true); }
    if (searchInfo.timeMs)           { report("time", true); }
    if (searchInfo.nodes)            { report("nodes", true); }
    if (searchInfo.nps)              { report("nps", true); }
    if (searchInfo.hashFull)         { report("hashfull", true); }
    if (searchInfo.cpuload)          { report("cpuload", true); }
    if (searchInfo.currMoveNumber)   { report("currmovenumber", true); }

}

QaplaBasics::Move PlayerContext::handleBestMove(const EngineEvent& event) {
    if (computeState_ != ComputeState::ComputingMove) {
        EngineLogger::engineLogger({.engineId = engine_->getIdentifier()}).
        log(std::format("{} Received best move while not computing a move, ignoring.", engine_->getIdentifier()), 
            TraceLevel::error);
        return {};
    }
    computeState_ = ComputeState::Idle;
    std::scoped_lock stateLock(stateMutex_);
    if (!logReport("legalmove", event.bestMove.has_value())) {
        gameState_.setGameResult(GameEndCause::IllegalMove, 
            gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
        std::scoped_lock lock(currentMoveMutex_);
        currentMove_ = MoveRecord(gameState_.getHalfmovesPlayed(), engine_->getIdentifier());
        return {};
    }
    
    const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
    if (!logReport("legalmove", !move.isEmpty(),
        std::format(R"(Encountered illegal move "{}" in bestmove, raw info line "{}")", *event.bestMove, event.rawLine))) {
        gameState_.setGameResult(GameEndCause::IllegalMove, 
            gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
        std::scoped_lock lock(currentMoveMutex_);
        currentMove_ = MoveRecord(gameState_.getHalfmovesPlayed(), engine_->getIdentifier());
        EngineLogger::engineLogger({.engineId = engine_->getIdentifier()}).
        log(std::format("{} Illegal move in bestmove: {} in raw info line \"{}\"", 
            engine_->getIdentifier(), *event.bestMove, event.rawLine), TraceLevel::info);
        return {};
    }
	
    if (isAnalyzing_) { return {}; }

    checkTime(event);
    // Must be calculated before doMove
    std::string san = gameState_.moveToSan(move);
    gameState_.doMove(move);
    engine_->bestMoveReceived(san, move.getLAN());

    std::scoped_lock curMoveLock(currentMoveMutex_);
    currentMove_.updateFromBestMove(gameState_.getHalfmovesPlayed(), engine_->getIdentifier(),
        event, move.getLAN(), san, computeMoveStartTimestamp_, 
        gameState_.getHalfmoveClock());
    return move;
}

void PlayerContext::handlePonderMove(const EngineEvent& event) {
    if (!event.ponderMove) {
        return;
    }

    std::scoped_lock lock(stateMutex_);
    ponderMove_ = *event.ponderMove;

    if (setupPonderState(ponderMove_, event.rawLine)) {
        computeState_ = ComputeState::PonderingOrIdle;
        // Update currentMove so GUI can display ponder move before PV (e.g., "e4 e5 Nc3...")
        {
            std::scoped_lock lock(currentMoveMutex_);
            currentMove_.ponderMove = ponderMove_;
        }
    } else {        
        ponderMove_.clear();
    }
}

bool PlayerContext::setupPonderState(const std::string& move, const std::string& rawLine) {
    // Validate that the ponder move is legal in the current position
    const auto parsedMove = gameState_.stringToMove(move, requireLan_);
    if (!logReport("legal-pondermove", !parsedMove.isEmpty(),
        std::format(R"(Received illegal ponder move "{}" from engine, raw line "{}")", 
            move, rawLine))) {
        return false;
    }

    // Create speculative ponder state
    ponderState_.synchronizeIncrementalFrom(gameState_);
    ponderState_.doMove(parsedMove);

    // Check if the game would be over after the ponder move
    auto [cause, result] = ponderState_.getGameResult();
    if (result != GameResult::Unterminated) {
        // Game would be over, cannot ponder
        ponderState_.undoMove();
        return false;
    }

    return true;
}

void PlayerContext::checkTime(const EngineEvent& event) {

    if (isAnalyzing_) { return; }
    const uint64_t GRACE_MS = 100;
    const uint64_t GRACE_NODES = 1000;
        
    const bool white = gameState_.isWhiteToMove();
    const uint64_t moveElapsedMs = event.timestampMs - computeMoveStartTimestamp_;
	currentMove_.timeMs = moveElapsedMs;

    const uint64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    int numLimits = static_cast<int>(goLimits_.hasTimeControl) 
        + static_cast<int>(goLimits_.moveTimeMs.has_value()) 
        + static_cast<int>(goLimits_.depth.has_value()) 
        + static_cast<int>(goLimits_.nodes.has_value());

    if (goLimits_.hasTimeControl) {
        if (!logReport("no-loss-on-time", moveElapsedMs <= timeLeft,
            std::format("Timecontrol: {} Used time: {} ms. Available Time: {} ms", 
                timeControl_.toPgnTimeControlString(), moveElapsedMs, timeLeft))) {
            gameState_.setGameResult(GameEndCause::Timeout, white ? GameResult::BlackWins : GameResult::WhiteWins);
        }
    }

    if (goLimits_.moveTimeMs.has_value()) {
        logReport("no-movetime-overrun", moveElapsedMs < *goLimits_.moveTimeMs + GRACE_MS,
            std::format("took {} ms, limit is {} ms", moveElapsedMs, *goLimits_.moveTimeMs), 
            TraceLevel::warning);
        if (numLimits == 1 && EngineReport::reportUnderruns) {
            logReport("no-movetime-underrun", moveElapsedMs > *goLimits_.moveTimeMs * 99 / 100,
                std::format("The engine should use EXACTLY {} ms but took {} ms", *goLimits_.moveTimeMs, moveElapsedMs), 
                TraceLevel::info);
        }
    }

    if (!event.searchInfo.has_value()) { return; }

    if (logReport("depth", event.searchInfo->depth.has_value())) {
        if (goLimits_.depth.has_value()) {
            uint32_t depth = *event.searchInfo->depth;
            logReport("no-depth-overrun", depth <= *goLimits_.depth,
                std::format("{} > {}", depth, *goLimits_.depth));
            if (numLimits == 1) {
                logReport("no-depth-underrun", depth >= *goLimits_.depth,
                    std::format("{} < {}", depth, *goLimits_.depth));
            }
        }
    }

    if (logReport("nodes", event.searchInfo->nodes.has_value())) {
        if (goLimits_.nodes.has_value()) {
            uint64_t nodes = *event.searchInfo->nodes;
            logReport("no-nodes-overrun", nodes <= *goLimits_.nodes + GRACE_NODES,
                std::format("{} > {}", nodes, *goLimits_.nodes));
            if (numLimits == 1) {
                logReport("no-nodes-underrun", nodes > *goLimits_.nodes * 9 / 10,
                    std::format("{} < {}", nodes, *goLimits_.nodes));
            }
        }
    }
}

bool PlayerContext::checkEngineTimeout() {
    if (computeState_ != ComputeState::ComputingMove) { return false; }
    if (!engine_) { return false; }
	if (isAnalyzing_) { return false; }

	const uint64_t GRACE_MS = 1000;
    const uint64_t OVERRUN_TIMEOUT = 5000;

    uint64_t moveElapsedMs = Timer::getCurrentTimeMs() - computeMoveStartTimestamp_;
    currentMove_.timeMs = moveElapsedMs;
    moveElapsedMs = moveElapsedMs < GRACE_MS ? 0 : moveElapsedMs - GRACE_MS;

    const bool white = gameState_.isWhiteToMove();
    bool restarted = false;

    const uint64_t timeLeft = white ? goLimits_.wtimeMs : goLimits_.btimeMs;
    bool overrun = false;

	if (goLimits_.hasTimeControl) {
        overrun = moveElapsedMs > timeLeft + OVERRUN_TIMEOUT;
        if (moveElapsedMs > timeLeft) {
			engine_->moveNow();
			restarted = restartIfNotReady(std::format(
                "engine exceeded its thinking time without sending a bestmove (elapsed {} ms, time left {} ms)",
                moveElapsedMs, timeLeft));
            gameState_.setGameResult(restarted ? GameEndCause::Disconnected : GameEndCause::Timeout, 
                white ? GameResult::BlackWins : GameResult::WhiteWins);
            if (!restarted) {
                logReport("no-loss-on-time", restarted, "Engine timeout and not reacting for a while, but answered isready");
            }
            EngineLogger::engineLogger({.engineId = engine_->getIdentifier()}).
            log(std::format("{} Engine timeout or disconnect", engine_->getIdentifier()), 
                TraceLevel::warning);
		}
	} else if ((goLimits_.moveTimeMs.has_value() && *goLimits_.moveTimeMs < moveElapsedMs)) {
        overrun = moveElapsedMs > *goLimits_.moveTimeMs + OVERRUN_TIMEOUT;
        engine_->moveNow();
        restarted = restartIfNotReady(std::format(
            "engine exceeded the fixed movetime without sending a bestmove (elapsed {} ms, movetime {} ms)",
            moveElapsedMs, *goLimits_.moveTimeMs));
    }
	if (overrun && !restarted) {
        // We are here, if the engine responded with isready but still does not play a move
        restartEngine(std::format(
            "engine answered isready but sent no bestmove for more than {} ms after its time ran out",
            OVERRUN_TIMEOUT));
        restarted = true;
	}
    if (restarted) {
        logReport("no-disconnect", !restarted, "Engine timeout and not reacting to isready, restarted ");
    }
    return restarted;
}

void PlayerContext::handleDisconnect(bool isWhitePlayer) {
    gameState_.setGameResult(GameEndCause::Disconnected, isWhitePlayer ? GameResult::BlackWins : GameResult::WhiteWins);
    logReport("no-disconnect", false, "Engine disconnected unexpectedly.");
    restartEngine("engine disconnected unexpectedly");
}

void PlayerContext::restartEngine(const std::string& reason, bool outside, TraceLevel traceLevel) {
	if (!engine_) {
		throw AppError::make("PlayerContext::restart; Cannot restart without an engine.");
	}
    if (!isEventQueueThread && !outside) {
		std::cerr
            << "PlayerContext::restartEngine called outside of the GameManager thread. This is not allowed.\n"
            << std::flush;
        throw AppError::make("PlayerContext::restart; Cannot restart engine outside of the GameManager thread.");
	}
    // Log the reason before replacing the engine so the message appears in the engine log
    // directly before the quit command sent to the old engine instance.
    const auto& eid = engine_->getIdentifier();
    EngineLogger::engineLogger({.engineId = eid}).log(
        std::format("{} Sending quit and restarting engine, reason: {}", eid, reason), traceLevel);
    computeState_ = ComputeState::Idle;
    // Create a fully initialized new engine instance (incl. UCI handshake)
    engine_ = EngineWorkerFactory::restart(*engine_);
}

bool PlayerContext::restartIfNotReady(const std::string& reason) {
    std::chrono::seconds WAIT_READY{ 1 };
	if (engine_ && !engine_->requestReady(WAIT_READY)) {
        restartEngine(reason + "; engine also did not answer isready within 1s");
		return true;
	}
    return false;
}

void PlayerContext::cancelCompute() {
    if (!engine_) { return; }
    constexpr auto readyTimeout = std::chrono::seconds{ 1 };
    if (computeState_ != ComputeState::Idle) {
        engine_->stopCompute(true);
        checkReady(readyTimeout);
    }
    computeState_ = ComputeState::Idle;
    ponderMove_.clear();
}

void PlayerContext::setStartPosition(const GameRecord& startPosition, bool engineIsWhite) {
    std::scoped_lock lock(stateMutex_);
    gameState_.setFromGameRecord(startPosition, startPosition.nextMoveIndex());
    ponderState_.setFromGameRecord(startPosition, startPosition.nextMoveIndex());
    setTimeControl(startPosition, engineIsWhite);
}

void PlayerContext::doMove(const MoveRecord& moveRecord) {
    const auto move = gameState_.stringToMove(moveRecord.original, false);
    doMove(move);
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
    if (isAssumedPondering()) {
        computeState_ = ponderMove_ == lanMove ? ComputeState::PonderHit : ComputeState::PonderMiss;
    }
    ponderMove_.clear();  

    if (computeState_ == ComputeState::PonderMiss) {
		auto success = engine_->handlePonderMiss();
        const auto& eid = engine_->getIdentifier();
        if (!logReport("correct-pondering", success,
            std::format("handling of ponder miss to engine (uci = stop) {} did not complete successfully", eid))) {
			EngineLogger::engineLogger({.engineId = eid}).
            log(std::format("{} handlePonderMiss did not complete in time", eid), TraceLevel::error);
			// Try to heal the situation by requesting a ready state from the engine
            engine_->requestReady();
        }
    }
    gameState_.doMove(move);
}

void PlayerContext::computeMove(const GameRecord& gameRecord, const GoLimits& goLimits, bool analyze) {
	if (!engine_) {
		throw AppError::make("PlayerContext::computeMove; Cannot compute move without an engine.");
	}
	if (computeState_ == ComputeState::ComputingMove) {
		throw AppError::make("PlayerContext::computeMove; Cannot compute move while already computing a move.");
	}

    {
        std::scoped_lock lock(currentMoveMutex_);
        if (computeState_ != ComputeState::PonderHit) {
            currentMove_.clear();
        }
        currentMove_.halfmoveNo_ = gameState_.getHalfmovesPlayed() + 1;
        currentMove_.engineName_ = engine_->getEngineName();
        currentMove_.ponderMove.clear();
		isAnalyzing_ = analyze;
    }
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
    const std::optional<EngineEvent>& event) 
{
    // Sets computeState_ = PonderingOrIdle in both UCI and XBoard cases, even though we have no ponder move
    // and the engine may not ponder at all.
    // XBoard never has a ponder move in the bestmove command, so we always send an empty ponder move
    // and the engine can only ignore this call.
    // We set ponderMove_, if we have a pondermove and we will clear it, if we have not.
    // Only computeState_ == PonderingOrIdle AND a non-empty ponderMove_ indicates that we actually ponder on a move.

	if (!engine_) {
		throw AppError::make("PlayerContext::allowPonder; Cannot allow pondering without an engine.");
	}
    if (!engine_->getConfig().isPonderEnabled()) { return; }
    if (!event) { return; }

	if (event->type != EngineEvent::Type::BestMove) {
		throw AppError::make("PlayerContext::allowPonder; Best move event required to ponder.");
	}
    if (computeState_ == ComputeState::ComputingMove) {
		throw AppError::make("PlayerContext::allowPonder; Cannot allow pondering while already computing a move.");
	}
	goLimits_ = goLimits;
    std::scoped_lock lock(stateMutex_);
    ponderMove_ = event->ponderMove ? *event->ponderMove : "";
    // Update currentMove so GUI can display ponder move before PV (e.g., "e4 e5 Nc3...")
    {
        std::scoped_lock lock(currentMoveMutex_);
        currentMove_.clear();
        currentMove_.halfmoveNo_ = gameState_.getHalfmovesPlayed() + 1;
        currentMove_.ponderMove = ponderMove_;
		isAnalyzing_ = false;
    }

    if (!ponderMove_.empty()) {
        if (setupPonderState(ponderMove_, event->rawLine)) {
            computeState_ = ComputeState::PonderingOrIdle;
            engine_->allowPonder(gameRecord, goLimits, ponderMove_);
        } else {
            // Setup failed (illegal move or game over), clear ponder move
            ponderMove_.clear();
        }
    }
    else {
        computeState_ = ComputeState::PonderingOrIdle;
		engine_->allowPonder(gameRecord, goLimits, ponderMove_);
    }

}

} // namespace QaplaTester
