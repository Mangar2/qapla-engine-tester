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

 /**
  * GameManager executes tasks such as playing games or calculating moves. Upon task completion, 
  * it queries its assigned TaskProvider (if any) for a new task.
  * If no task is available, it requests a new TaskProvider from GameManagerPool.
  *
  * The pool manages all GameManagers and a list of active TaskProviders. 
  * When providing a new TaskProvider, it also returns the first available task.
  * This ensures that a parallel GameManager cannot intercept the next task before the requesting 
  * GameManager can retrieve it.
  *
  * This coordination avoids race conditions where a new TaskProvider would otherwise appear empty. 
  * Providers receive result updates to support
  * dynamic control (e.g., stopping ongoing tasks when target results are achieved).
  */

#include "game-manager.h"
#include "engine-report.h"
#include <iostream>
#include "game-manager-pool.h"
#include "input-handler.h"
#include "adjucation-manager.h"

GameManager::GameManager(): taskProvider_(nullptr) {
    eventThread_ = std::thread(&GameManager::processQueue, this);
    gameContext_.setEventCallback([this](EngineEvent&& event) {
        enqueueEvent(std::move(event));
        });
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

void GameManager::processQueue() {
    constexpr std::chrono::seconds timeoutInterval(1);
    auto nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;
    isEventQueueThread = true;

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
            if (debug_) std::cout << "Timeout check" << std::endl;
            nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;

            if (taskType_ != GameTask::Type::ComputeMove && taskType_ != GameTask::Type::PlayGame) {
                if (debug_) std::cout << "Stop check, cause task-type" << std::to_string(static_cast<int>(taskType_.load())) << std::endl;
                continue;
            }
			bool restarted = gameContext_.checkForTimeoutsAndRestart();

            if (checkForGameEnd() || (restarted && taskType_ != GameTask::Type::PlayGame)) {
                computeNextTask();
            }
        }
    }
}

void GameManager::tearDown() {
    if (taskProvider_) {
        taskProvider_ = nullptr;
    }
	gameContext_.tearDown();
	markFinished();
}

void GameManager::markFinished() {
	taskProvider_ = nullptr; 
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

void GameManager::markRunning() {
	if (!finishedPromiseValid_) {
		finishedPromise_ = std::promise<void>();
		finishedFuture_ = finishedPromise_.get_future();
		finishedPromiseValid_ = true;
	}
}

void GameManager::processEvent(const EngineEvent& event) {
    try {
		PlayerContext* player = gameContext_.findPlayerByEngineId(event.engineIdentifier);
		bool isWhitePlayer = player == gameContext_.getWhite();

        if (!player) {
            // Usally from an engine in termination process. E.g. we stop an engine not reacting and already
            // Started new engines but the old engine still sends data.
            return;
        }

        // Error reporting
		std::string name = player->getEngine()->getConfig().getName();
		EngineReport* checklist = EngineReport::getChecklist(name);
        for (auto& error : event.errors) {
            checklist->logReport(error.name, false, error.detail, error.level);
        }

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
        if (event.type == EngineEvent::Type::SendingComputeMove) {
            player->setComputingMove(true);
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
	PlayerContext* player = gameContext_.findPlayerByEngineId(event.engineIdentifier);

    if (player) {
        move = player->handleBestMove(event);
        moveRecord = player->getCurrentMove();
    }
	if (move != QaplaBasics::Move::EMPTY_MOVE) {
        auto& gameRecord = gameContext_.gameRecord();
		gameRecord.addMove(moveRecord);
        PlayerContext* opponent = player == gameContext_.getWhite() ? gameContext_.getBlack() : gameContext_.getWhite();

		if (player != opponent) {
            opponent->doMove(move);
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
	bool stopRequired = taskProvider_->setPV(taskId_, pv, 
        event.timestampMs < start ? 0 : event.timestampMs - start, 
        event.searchInfo->depth, event.searchInfo->nodes, event.searchInfo->multipv);
    if (stopRequired) {
        moveNow();
    }
}

std::tuple<GameEndCause, GameResult> GameManager::getGameResult() {
	auto [cause, result] = gameContext_.checkGameResult();
    
    // If any player detects a game  - end return it. 
	if (cause != GameEndCause::Ongoing) return { cause, result };

	auto& gameRecord = gameContext_.gameRecord();

	AdjudicationManager::instance().testAdjudicate(gameRecord);

	auto [dcause, dresult] = AdjudicationManager::instance().adjudicateDraw(gameRecord);
    if (dresult != GameResult::Unterminated) return { dcause, dresult };

	auto [rcause, rresult] = AdjudicationManager::instance().adjudicateResign(gameRecord);
	if (rresult != GameResult::Unterminated) return { rcause, rresult };

	return { GameEndCause::Ongoing, GameResult::Unterminated };
}

bool GameManager::checkForGameEnd() {
    // Both player should have the right result but the player not to move is still passive
    auto [cause, result] = getGameResult();

    if (result == GameResult::Unterminated) {
        return false;
    }
    auto& gameRecord = gameContext_.gameRecord();
	gameRecord.setGameEnd(cause, result);

	Logger::testLogger().log("[Result: " + gameResultToPgnResult(result) + "]", TraceLevel::info);
	Logger::testLogger().log("[Termination: " + gameEndCauseToPgnTermination(cause) + "]", TraceLevel::info);

    return true;
}

void GameManager::moveNow() {
    if (gameContext_.getPlayerCount() == 0) return;

    auto& gameRecord = gameContext_.gameRecord();

    if (gameRecord.isWhiteToMove()) {
        gameContext_.getWhite()->getEngine()->moveNow();
    }
    else {
        gameContext_.getBlack()->getEngine()->moveNow();
    }
}

void GameManager::computeNextMove(const std::optional<EngineEvent>& event) {
    auto& gameRecord = gameContext_.gameRecord();
	auto white = gameContext_.getWhite();
	auto black = gameContext_.getBlack();
    auto [whiteTime, blackTime] = gameRecord.timeUsed();
    GoLimits goLimits = createGoLimits(
		white->getTimeControl(), black->getTimeControl(),
        gameRecord.nextMoveIndex(), whiteTime, blackTime, gameRecord.isWhiteToMove());
	if (gameRecord.isWhiteToMove()) {
        white->computeMove(gameRecord, goLimits);
        black->allowPonder(gameRecord, goLimits, event);
    }
    else {
		black->computeMove(gameRecord, goLimits);
        white->allowPonder(gameRecord, goLimits, event);
    }
}

std::optional<GameTask> GameManager::tryGetReplacementTask() {
    auto extendedTask = GameManagerPool::getInstance().tryAssignNewTask();
    if (!extendedTask) {
        return std::nullopt;
    }

    taskProvider_ = extendedTask->provider;

    if (extendedTask->black) {
        initEngines(
            std::move(extendedTask->white),
            std::move(extendedTask->black));
    }
    else {
        initUniqueEngine(std::move(extendedTask->white));
    }

    return extendedTask->task;
}

std::optional<GameTask> GameManager::organizeNewAssignment() {
    if (!taskProvider_) {
        // No taskProvider_ means no task assignment. And a GameManager without assignment is inactive.
        // Therefore, no attempt is made to request a new TaskProvider.
        return std::nullopt;
    }
    // The GameManagerPool may reduce the number of active GameManagers (e.g. from 10 to 8).
    // To do this, it checks how many GameManagers are currently active,
    // where "active" is defined as having a non-null taskProvider_.
    // If there are too many, the pool deactivates individual GameManagers by setting their
    // taskProvider_ to nullptr.
    //
    // This deactivation is performed via GameManagerPool::clearIfNecessary(), which ensures
    // that the counting of active GameManagers and the selection of those to be cleared
    // is done atomically. A mutex guards this process to prevent multiple GameManagers from
    // being deactivated concurrently due to a race in active-count evaluation.
    //
    // Note: taskProvider_ itself is only accessed by the owning GameManager and does not
    // require internal synchronization. However, the pool must synchronize the decision-making
    // process across GameManagers to avoid clearing more instances than intended.
	if (GameManagerPool::getInstance().maybeDeactivateManager(taskProvider_)) {
        return std::nullopt;
    }

    auto task = taskProvider_->nextTask();
    if (task) {
        gameContext_.restartIfConfigured();
        return task;
    }
    // tryGetReplacementTask already provides new engine instances so restarting is not needed.
    return tryGetReplacementTask();
}

void GameManager::computeTask(std::optional<GameTask> task) {
    if (!task) {
		tearDown();
        return;
    }
	gameContext_.setSideSwitched(task->switchSide);
	auto& gameRecord = gameContext_.gameRecord();

    // Also sets the engines names, Switched side must be set before
	setFromGameRecord(gameRecord);
    setTimeControls(gameRecord.getWhiteTimeControl(), gameRecord.getBlackTimeControl());
	taskType_ = task->taskType;
	taskId_ = task->taskId;
    // Notify engines that a new game or task is starting to allow reset of internal state (e.g., memory, hash tables)
    gameContext_.newGame();
	computeNextMove();
}

void GameManager::stop() {
    taskType_ = GameTask::Type::None;
    gameContext_.cancelCompute();

    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        while (!eventQueue_.empty()) {
            eventQueue_.pop();
        }
    }
	tearDown();
}

void GameManager::computeNextTask() {
    if (taskType_ == GameTask::Type::None) {
        // Already processed to end
        return;
    }
    taskType_ = GameTask::Type::None;
	gameContext_.cancelCompute();

    while (!eventQueue_.empty()) {
        eventQueue_.pop();
    }

	if (!taskProvider_) {
        tearDown();
		return;
	}
    // Note: we had a check, if any move has been played and removed it as it could cause problems
    // With a direct loss e.g. due to disconnect. But I don´t know why we ever checked for any move
	auto& gameRecord = gameContext_.gameRecord();
    taskProvider_->setGameRecord(taskId_, gameRecord);
	AdjudicationManager::instance().onGameFinished(gameRecord);
    auto task = organizeNewAssignment();
    if (!task) {
		tearDown();
        return;
    }

	computeTask(std::move(task));
}

bool GameManager::computeTasks(std::shared_ptr<GameTaskProvider> taskProvider) {
    std::optional<GameTask> task;
    if (taskProvider == nullptr) {
        task = tryGetReplacementTask();
    }
    else {
        taskProvider_ = std::move(taskProvider);
        taskType_ = GameTask::Type::FetchNextTask;
        task = organizeNewAssignment();
    }
    if (task) {
        markRunning();
        computeTask(std::move(task));
		return true;
    }
    return false;
}



