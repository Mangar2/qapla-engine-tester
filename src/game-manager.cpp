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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#include "game-manager.h"
#include "checklist.h"
#include <iostream>

GameManager::GameManager(): taskProvider_(nullptr) {
    eventThread_ = std::thread(&GameManager::processQueue, this);
    whitePlayer_ = &player1_;
    blackPlayer_ = &player2_;
}

GameManager::~GameManager() {
    stopThread_ = true;
    queueCondition_.notify_all();
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
}

void GameManager::enqueueEvent(const EngineEvent& event) {
	if (taskType_ == GameTask::Type::None) {
		// No task to process, ignore the event
		return;
	}
    if (event.type == EngineEvent::Type::None || event.type == EngineEvent::Type::NoData) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.push(event);
    }
    queueCondition_.notify_one();
}

bool GameManager::processNextEvent() {
	if (taskType_ == GameTask::Type::None) {
		return false; // No task to process
	}
    EngineEvent event;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (eventQueue_.empty()) {
            return false;
        }
        event = std::move(eventQueue_.front());
        eventQueue_.pop();
    }
    processEvent(event);
    return true;
}

void GameManager::setUniqueEngine(std::unique_ptr<EngineWorker> engine) {
    engine->setEventSink([this](EngineEvent&& event) {
        enqueueEvent(std::move(event));
        });
    whitePlayer_ = &player1_;
    blackPlayer_ = &player1_;
    whitePlayer_->setEngine(std::move(engine), requireLan_);
}

void GameManager::setEngines(std::unique_ptr<EngineWorker> white, std::unique_ptr<EngineWorker> black) {
    white->setEventSink([this](EngineEvent&& event) {
        enqueueEvent(std::move(event));
        });

    black->setEventSink([this](EngineEvent&& event) {
        enqueueEvent(std::move(event));
        });

    whitePlayer_ = &player1_;
    blackPlayer_ = &player2_;
	whitePlayer_->setEngine(std::move(white), requireLan_);
    blackPlayer_->setEngine(std::move(black), requireLan_);
}

void GameManager::processQueue() {
    constexpr std::chrono::seconds timeoutInterval(1);
    auto nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;

    while (!stopThread_) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCondition_.wait_until(lock, nextTimeoutCheck, [this] {
                return !eventQueue_.empty() || stopThread_;
                });
        }

        while (processNextEvent()) {
            // Process all pending events
        }

        if (std::chrono::steady_clock::now() >= nextTimeoutCheck) {
            nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;

            if (taskType_ != GameTask::Type::ComputeMove && taskType_ != GameTask::Type::PlayGame) {
                continue;
            }
            bool restarted = false;
            if (whitePlayer_->checkEngineTimeout()) {
                restarted = true;
                whitePlayer_->getEngine()->setEventSink([this](EngineEvent&& event) {
                    enqueueEvent(std::move(event));
                    });;
            }
            if (whitePlayer_ != blackPlayer_) {
                if (blackPlayer_->checkEngineTimeout()) {
					restarted = true;
                    blackPlayer_->getEngine()->setEventSink([this](EngineEvent&& event) {
                        enqueueEvent(std::move(event));
                        });;
                }
            }
            if (checkForGameEnd() || (restarted && taskType_ != GameTask::Type::PlayGame)) {
                computeNextTask();
            }
        }
    }
}

void GameManager::switchSide() {
    std::swap(whitePlayer_, blackPlayer_);
	switchedSide_ = !switchedSide_;
}

void GameManager::markFinished() {
    if (finishedPromiseValid_) {
        try {
            finishedPromise_.set_value();
        }
        catch (const std::future_error&) {
            // already satisfied � ignore or log
        }
        finishedPromiseValid_ = false;
    }
}

void GameManager::processEvent(const EngineEvent& event) {
    try {

        for (auto& error : event.errors) {
            Checklist::logCheck(error.name, false, error.detail, TraceLevel::info);
        }
		bool isWhitePlayer = event.engineIdentifier == whitePlayer_->getEngine()->getIdentifier();
		bool isBlackPlayer = event.engineIdentifier == blackPlayer_->getEngine()->getIdentifier();
        if (!isWhitePlayer && !isBlackPlayer) {
            // Usally from an engine in termination process. E.g. we stop an engine not reacting and already
            // Started new engines but the old engine still sends data.
            return;
        }
        PlayerContext* player = isWhitePlayer ? whitePlayer_ : blackPlayer_;

        if (event.type == EngineEvent::Type::EngineDisconnected) {
            player->handleDisconnect(isWhitePlayer);
            player->getEngine()->setEventSink([this](EngineEvent&& event) {
                enqueueEvent(std::move(event));
                });
            if (taskType_ != GameTask::Type::PlayGame) {
                computeNextTask();
                return;
            }
		}

        if (event.type == EngineEvent::Type::ComputeMoveSent) {
            // We get the start calculating move timestamp directly from the EngineProcess after sending the compute move string
            // to the engine. This prevents loosing time for own synchronization tasks on the engines clock.
            player->setComputeMoveStartTimestamp(event.timestampMs);
            return;
        }

        if (event.type == EngineEvent::Type::BestMove) {
            handleBestMove(event);
            if (taskType_ == GameTask::Type::ComputeMove) {
                computeNextTask();
                return;
            }
        }

        if (event.type == EngineEvent::Type::Info) {
            informTask(event, player);
            player->handleInfo(event);
        }

        if (taskType_ == GameTask::Type::PlayGame) {
            if (checkForGameEnd()) {
                computeNextTask();
                return;
            }
            if (event.type == EngineEvent::Type::BestMove) {
                computeNextMove(event);
                return;
            }
        }

    }
	catch (const std::exception& e) {
		Logger::testLogger().log("Exception in GameManager::handleState " + std::string(e.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception in GameManager::handleState", TraceLevel::error);
	}
}

void GameManager::handleBestMove(const EngineEvent& event) {
    QaplaBasics::Move move;
	MoveRecord moveRecord;
	PlayerContext* playerToInform = nullptr;
    PlayerContext* player = nullptr;
    // Special feature for the test output of a single game played
    if (logMoves_) std::cout << *event.bestMove << " " << std::flush;
	if (whitePlayer_->getEngine()->getIdentifier() == event.engineIdentifier) {
		player = whitePlayer_;
        playerToInform = blackPlayer_;
	}
	else if (blackPlayer_->getEngine()->getIdentifier() == event.engineIdentifier) {
		player = blackPlayer_;
        playerToInform = whitePlayer_;
	}
    if (player) {
        move = player->handleBestMove(event);
        moveRecord = player->getCurrentMove();
    }
	if (move != QaplaBasics::Move::EMPTY_MOVE) {
		gameRecord_.addMove(moveRecord);
		if (player != playerToInform) {
            playerToInform->doMove(move);
		}
	}

}

void GameManager::informTask(const EngineEvent& event, const PlayerContext* player) {
	if (!taskProvider_) {
		return; // No task provider set, nothing to inform
	}
	if (event.type != EngineEvent::Type::Info || !event.searchInfo) {
		return; // Only interested in info events
	}
	auto pv = event.searchInfo->pv;
	if (pv.empty()) {
		return; // No principal variation to set
	}
    auto start = player->getComputeMoveStartTimestamp();
	bool stopRequired = taskProvider_->setPV(event.engineIdentifier, pv, 
        event.timestampMs < start ? 0 : event.timestampMs - start, 
        event.searchInfo->depth, event.searchInfo->nodes, event.searchInfo->multipv);
    if (stopRequired) {
        moveNow();
    }
}

std::tuple<GameEndCause, GameResult> GameManager::getGameResult() {
    auto [wcause, wresult] = whitePlayer_->getGameResult();
    auto [bcause, bresult] = blackPlayer_->getGameResult();
    
    // Timeout or Disconneced is only detected by the loosing player
    if (wcause == GameEndCause::Timeout || wcause == GameEndCause::Disconnected) {
		return { wcause, wresult };
    }
    else if (bcause == GameEndCause::Timeout || bcause == GameEndCause::Disconnected) {
        return { bcause, bresult };
    }
    // We take the result from the player doing the last move (thus not at move)
    if (gameRecord_.isWhiteToMove()) {
		return { bcause, bresult };
	}
    else {
        return { wcause, wresult };
    }
}

bool GameManager::checkForGameEnd() {
    // Both player should have the right result but the player not to move is still passive
    auto [cause, result] = getGameResult();

    if (result == GameResult::Unterminated) {
        return false;
    }

	gameRecord_.setGameEnd(cause, result);
    if (logMoves_) std::cout << "\n";
	Logger::testLogger().log("[Result: " + gameResultToPgnResult(result) + "]", TraceLevel::info);
	Logger::testLogger().log("[Termination: " + gameEndCauseToPgnTermination(cause) + "]", TraceLevel::info);

    return true;
}

void GameManager::moveNow() {
    if (gameRecord_.isWhiteToMove()) {
		whitePlayer_->getEngine()->moveNow();
    }
    else {
		blackPlayer_->getEngine()->moveNow();
    }
}

void GameManager::computeMove(bool useStartPosition, const std::string fen) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
	taskProvider_ = nullptr;
	setStartPosition(useStartPosition, fen);
	gameRecord_.setTimeControl(whitePlayer_->getTimeControl(), blackPlayer_->getTimeControl());
	taskType_ = GameTask::Type::ComputeMove;
    logMoves_ = false;
    computeNextMove();
}

void GameManager::computeNextMove(const std::optional<EngineEvent>& event) {
    auto [whiteTime, blackTime] = gameRecord_.timeUsed();
    GoLimits goLimits = createGoLimits(
		whitePlayer_->getTimeControl(), blackPlayer_->getTimeControl(),
        gameRecord_.nextMoveIndex(), whiteTime, blackTime, gameRecord_.isWhiteToMove());
	if (gameRecord_.isWhiteToMove()) {
        whitePlayer_->computeMove(gameRecord_, goLimits);
        blackPlayer_->allowPonder(gameRecord_, goLimits, event);
    }
    else {
		blackPlayer_->computeMove(gameRecord_, goLimits);
        whitePlayer_->allowPonder(gameRecord_, goLimits, event);
    }
}

void GameManager::computeGame(bool useStartPosition, const std::string fen, bool logMoves) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
	taskProvider_ = nullptr;
	setStartPosition(useStartPosition, fen);
    taskType_ = GameTask::Type::PlayGame;
	logMoves_ = logMoves;
    computeNextMove();
}

void GameManager::computeNextTask() {
    if (taskType_ == GameTask::Type::None) {
        // Already processed to end
        return;
    }
    taskType_ = GameTask::Type::None;

    whitePlayer_->cancelCompute();
    if (blackPlayer_ != whitePlayer_) {
        blackPlayer_->cancelCompute();
    }
    while (!eventQueue_.empty()) {
        eventQueue_.pop();
    }

	if (!taskProvider_) {
        finishedPromise_.set_value();
		return;
	}
	auto whiteId = whitePlayer_->getEngine()->getIdentifier();
	auto blackId = blackPlayer_->getEngine()->getIdentifier();
    if (gameRecord_.nextMoveIndex() > 0) {
        taskProvider_->setGameRecord(whiteId, blackId, gameRecord_);
    }
    auto newTask = taskProvider_->nextTask(whiteId, blackId);
	if (!newTask) {
        taskType_ = GameTask::Type::None;
		finishedPromise_.set_value();
		return;
	}
	auto task = *newTask;
	if (task.switchSide != switchedSide_) {
		switchSide();
	}
	setStartPosition(task.useStartPosition, task.fen);
	gameRecord_.setTimeControl(task.whiteTimeControl, task.blackTimeControl);
	gameRecord_.setRound(task.round);
	setTimeControls(task.whiteTimeControl, task.blackTimeControl);
    taskType_ = task.taskType;

    computeNextMove();
}

void GameManager::computeTasks(GameTaskProvider* taskProvider) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
	taskProvider_ = std::move(taskProvider);
    logMoves_ = false;
	taskType_ = GameTask::Type::FetchNextTask;
    computeNextTask();
}

void GameManager::run() {
}