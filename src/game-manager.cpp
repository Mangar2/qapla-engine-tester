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

#include "game-manager.h"
#include "engine-checklist.h"
#include <iostream>

GameManager::GameManager() {
    heartBeat_ = std::make_unique<HeartBeat>([this]() {
        EngineEvent event;
        event.type = EngineEvent::Type::KeepAlive;
        event.timestampMs = Timer::getCurrentTimeMs();
        handleState(event);
        });
}

void GameManager::setUniqueEngine(std::shared_ptr<EngineWorker> engine) {
    whiteEngine_ = engine;
    blackEngine_ = engine;

    engine->setEventSink([this](const EngineEvent& event) {
        handleState(event);
        });
}

void GameManager::setEngines(std::shared_ptr<EngineWorker> white, std::shared_ptr<EngineWorker> black) {
    whiteEngine_ = std::move(white);
    blackEngine_ = std::move(black);

    whiteEngine_->setEventSink([this](const EngineEvent& event) {
        handleState(event);
        });

    if (blackEngine_ != whiteEngine_) {
        blackEngine_->setEventSink([this](const EngineEvent& event) {
            handleState(event);
            });
    }
}

void GameManager::switchSide() {
    std::swap(whiteEngine_, blackEngine_);
	std::swap(whiteTimeControl_, blackTimeControl_);
}

void GameManager::markFinished() {
    // Verhindert Ausnahme bei mehrfacher set_value()
    if (finishedPromiseValid_) {
        try {
            finishedPromise_.set_value();
        }
        catch (const std::future_error&) {
            // already satisfied – ignorieren oder loggen
        }
        finishedPromiseValid_ = false;
    }
}

void GameManager::handleState(const EngineEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    for (auto& error: event.errors) {
        handleCheck(error.name, false, error.detail);
    }
    if (event.type == EngineEvent::Type::KeepAlive) {
    }
    else if (event.type == EngineEvent::Type::BestMove) {
        handleBestMove(event);
        if (task_ == Tasks::ComputeMove) {
            finishedPromise_.set_value();
			task_ = Tasks::None;
        } 
        else if (task_ == Tasks::PlayGame) {
            if (checkForGameEnd()) {
                finishedPromise_.set_value();
			}
            else {
                computeMove();
            }
        } 
        else if (task_ == Tasks::ParticipateInTournament) {
            if (checkForGameEnd()) {
                computeNextGame();
            }
			else {
				computeMove();
			}
        }
    }
    else if (event.type == EngineEvent::Type::Info) {
        handleInfo(event);
    }
    else if (event.type == EngineEvent::Type::ComputeMoveSent) {
        // We get the start calculating move timestamp directly from the EngineProcess after sending the compute move string
		// to the engine. This prevents loosing time for own synchronization tasks on the engines clock.
		computeMoveStartTimestamp_ = event.timestampMs;
    }
}

void GameManager::handleHeartBeat() {

}

void GameManager::handleBestMove(const EngineEvent& event) {
    if (!handleCheck("Computing a move returns a move check", event.bestMove.has_value())) return;
    if (logMoves_) std::cout << *event.bestMove << " " << std::flush;
	const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
    if (!handleCheck("Computing a move returns a legal move check", !move.isEmpty(),
        "Encountered illegal move " + *event.bestMove + " in currMove, raw info line " + event.rawLine)) {
		gameState_.setGameResult(GameEndCause::IllegalMove, gameState_.isWhiteToMove() ? GameResult::BlackWins : GameResult::WhiteWins);
        return;
    }
    checkTime(event);
    gameState_.doMove(move);
	currentMove_.updateFromBestMove(event, computeMoveStartTimestamp_);
	gameRecord_.addMove(currentMove_);
}

void GameManager::handleInfo(const EngineEvent& event) {
	if (!event.searchInfo.has_value()) return;
    const auto& searchInfo = *event.searchInfo;

    currentMove_.updateFromSearchInfo(searchInfo);

    // Prüfe currMove, falls vorhanden
    if (searchInfo.currMove) {
        const auto move = gameState_.stringToMove(*searchInfo.currMove, requireLan_);
        handleCheck("currmove check", !move.isEmpty(),
            "Encountered illegal move " + *searchInfo.currMove + " in currMove, raw info line " + event.rawLine);
    }

    // Prüfe PV, falls vorhanden
    if (!searchInfo.pv.empty()) {
        std::vector<QaplaBasics::Move> pvMoves;
        pvMoves.reserve(searchInfo.pv.size());

        bool valid = true;
        for (size_t i = 0; i < searchInfo.pv.size(); ++i) {
            const auto& moveStr = searchInfo.pv[i];
            const auto move = gameState_.stringToMove(moveStr, requireLan_);
            if (move.isEmpty()) {
                std::string fullPv;
                for (const auto& m : searchInfo.pv)
                    fullPv += m + " ";
                if (!fullPv.empty()) fullPv.pop_back(); // letztes Leerzeichen entfernen
                handleCheck("PV check", true,
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
            handleCheck("PV check", true); 
    }
}

bool GameManager::checkForGameEnd() {
	auto [cause, result] = gameState_.getGameResult();
    if (result == GameResult::Unterminated) {
        return false;
    }
    if (logMoves_) std::cout << "\n";
	Logger::testLogger().log("[Result: " + gameResultToPgnResult(result) + "]", TraceLevel::commands);
	Logger::testLogger().log("[Termination: " + gameEndCauseToPgnTermination(cause) + "]", TraceLevel::commands);

    return true;
}

void GameManager::checkTime(const EngineEvent& event) {
	const int64_t GRACE_MS = 5; // ms
    const int64_t GRACE_NODES = 1000;
    
    const GoLimits limits = currentGoLimits_;
    const bool white = gameState_.isWhiteToMove();
    const int64_t moveElapsedMs = event.timestampMs - computeMoveStartTimestamp_;

     // Overrun ist always an error, every limit must be respected
    const int64_t timeLeft = white ? limits.wtimeMs : limits.btimeMs;

	int numLimits = (timeLeft > 0) + limits.movetimeMs.has_value() +
		limits.depth.has_value() + limits.nodes.has_value();

    if (timeLeft > 0) {
        if (!handleCheck("wtime/btime overrun check", moveElapsedMs <= timeLeft,
            std::to_string(moveElapsedMs) + " > " + std::to_string(timeLeft))) {
			gameState_.setGameResult(GameEndCause::Timeout, white ? GameResult::BlackWins : GameResult::WhiteWins);
        }
    }

	if (limits.movetimeMs.has_value()) {
		handleCheck("movetime overrun check", moveElapsedMs < *limits.movetimeMs + GRACE_MS,
			std::to_string(moveElapsedMs) + " < " + std::to_string(*limits.movetimeMs));
        if (numLimits == 1) {
            handleCheck("movetime underrun check", moveElapsedMs > *limits.movetimeMs * 99 / 100,
               "The engine should use EXACTLY " + std::to_string(*limits.movetimeMs) + 
                " ms but took " + std::to_string(moveElapsedMs));
        }
	}

	if (!event.searchInfo.has_value()) return;

    if (handleCheck("Engine provides search depth info", event.searchInfo->depth.has_value())) {
        if (limits.depth.has_value()) {
            int depth = *event.searchInfo->depth;
            handleCheck("depth overrun check", depth <= *limits.depth,
                std::to_string(depth) + " > " + std::to_string(*limits.depth));
            if (numLimits == 1) {
                handleCheck("depth underrun check", depth >= *limits.depth,
                    std::to_string(depth) + " > " + std::to_string(*limits.depth));
            }
        }
    }

    if (handleCheck("Engine provides nodes info", event.searchInfo->nodes.has_value())) {
        if (limits.nodes.has_value()) {
			int64_t nodes = *event.searchInfo->nodes;
            handleCheck("nodes overrun check", nodes <= *limits.nodes + GRACE_NODES,
                std::to_string(nodes) + " > " + std::to_string(*limits.nodes));
            if (numLimits == 1) {
                handleCheck("nodes underrun check", nodes > *limits.nodes * 9 / 10,
                    std::to_string(nodes) + " > " + std::to_string(*limits.nodes));
            }
        }
    }

}

void GameManager::computeMove(bool startPos, const std::string fen) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
	gameState_.setFen(startPos, fen);
    gameRecord_.newGame(startPos, fen);
	task_ = Tasks::ComputeMove;
    logMoves_ = false;
    computeMove();
}

void GameManager::computeMove() {
    auto [whiteTime, blackTime] = gameRecord_.timeUsed();
    currentGoLimits_ = createGoLimits(
        whiteTimeControl_, blackTimeControl_, gameRecord_.currentPly(), whiteTime, blackTime, gameState_.isWhiteToMove());
	if (gameState_.isWhiteToMove()) {
		whiteEngine_->computeMove(gameRecord_, currentGoLimits_);
    }
    else {
		blackEngine_->computeMove(gameRecord_, currentGoLimits_);
    }
}

void GameManager::computeGame(bool startPos, const std::string fen, bool logMoves) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
    gameState_.setFen(startPos, fen);
    gameRecord_.newGame(startPos, fen);
    task_ = Tasks::PlayGame;
	logMoves_ = logMoves;
    computeMove();
}

void GameManager::computeNextGame() {
    auto newTask = taskProvider_();
	if (!newTask.has_value()) {
		finishedPromise_.set_value();
		return;
	}
	auto task = *newTask;
	gameState_.setFen(task.useStartPosition, task.fen);
	gameRecord_.newGame(task.useStartPosition, task.fen);
	setTimeControls(task.whiteTimeControl, task.blackTimeControl);
    computeMove();
}

void GameManager::computeGames(std::function<std::optional<GameTask>()> taskProvider) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
    task_ = Tasks::ParticipateInTournament;
	taskProvider_ = std::move(taskProvider);
    logMoves_ = false;
    computeNextGame();
}

bool GameManager::isLegalMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    return !move.isEmpty();
}

void GameManager::run() {
}