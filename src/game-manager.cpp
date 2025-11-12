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
#include "game-manager-pool.h"
#include "input-handler.h"
#include "adjudication-manager.h"
#include "engine-worker-factory.h"

#include <iostream>
#include <atomic>

namespace QaplaTester {

GameManager::GameManager(GameManagerPool* pool)
    : taskProvider_(nullptr), pool_(pool)
{
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

void GameManager::enqueueEvent(EngineEvent&& event) {
	if (managerState_ == ManagerState::None) {
		// No task to process, ignore the event
		return;
	}
    // Type::None is an empty event (uninitialized) or dedicately set to noop. Shouldnt happen but we ignore it here.
    // Type::NoData is an event from the engine, produced by a read with no relevant data (infos, ...).
    // we could ignore it at engine level but we decided that it is the responsibility of the GameManager to ignore such events.
    if (event.type == EngineEvent::Type::None || event.type == EngineEvent::Type::NoData) {
        return;
    }
    {
        std::scoped_lock lock(queueMutex_);
        eventQueue_.push(std::move(event));
    }
    queueCondition_.notify_one();
}

bool GameManager::processNextEvent() {
    EngineEvent event;
    {
        std::scoped_lock lock(queueMutex_);
        if (eventQueue_.empty()) {
            return false;
        }
        event = std::move(eventQueue_.front());
        eventQueue_.pop();
    }
    // Handles start/stop events not provided by engines
    if (event.type == EngineEvent::Type::StopRunning) {
        tearDown("processNextEvent/StopRunning");
        return true;
    }
    if (event.type == EngineEvent::Type::StartTask) {
        auto task = assignNewProviderAndTask();
        if (task) {
            executeTask(std::move(task));
            return true;
        }
        tearDown("processNextEvent/StartTask no task");
        return true;
    }
    processEvent(event);
    return true;
}

void GameManager::processQueue() {
    constexpr std::chrono::seconds timeoutInterval(1);
    auto nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;
    // thread local flag used for debugging purposes, checks if we have any other thread
    // restarting the engine than this here. As it is thread local it is also GameManager local.
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
            if (debug_) {
                std::cout << "Timeout check\n";
            }
            nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;

            // Only check for timeouts during engine game/move
            if (managerState_ != ManagerState::ComputeMove && managerState_ != ManagerState::PlayGame) {
                continue;
            }

			bool engineRestarted = gameContext_.checkForTimeoutsAndRestart();

            if (checkForGameEnd() || (engineRestarted && managerState_ != ManagerState::PlayGame)) {
                finalizeTaskAndContinue();
            }
        }
    }
}

void GameManager::tearDown([[maybe_unused]] const char* who) {
    // who is used for tracing that is currently not active
    if (!isRunning()) {
        return;
    }
    taskProvider_ = nullptr;
    // Terminates and deletes all engines
	gameContext_.tearDown();
    managerState_ = ManagerState::NotRunning;
	signalFinished();
}

void GameManager::signalFinished() {
    if (finishedPromiseValid_) {
        try {
            finishedPromise_.set_value();
        }
        catch (const std::future_error&) {
            // already satisfied -> ignore or log
        }
        finishedPromiseValid_ = false;
    }
}

void GameManager::initializeFinishedFuture() {
	if (!finishedPromiseValid_) {
		finishedPromise_ = std::promise<void>();
		finishedFuture_ = finishedPromise_.get_future();
		finishedPromiseValid_ = true;
	}
}

void GameManager::handleEngineDisconnect(PlayerContext* player, bool isWhitePlayer) {
    player->handleDisconnect(isWhitePlayer);
    player->getEngine()->setEventSink([this](EngineEvent&& curEvent) {
        enqueueEvent(std::move(curEvent));
    });
}

void GameManager::clearQueueButHandleDisconnects() {
    // Process disconnect events even when clearing the queue
    // This is critical when both engines disconnect - we need to restart both
    while (!eventQueue_.empty()) {
        auto& event = eventQueue_.front();
        if (event.type == EngineEvent::Type::EngineDisconnected) {
            PlayerContext* player = gameContext_.findPlayerByEngineId(event.engineIdentifier);
            if (player != nullptr) {
                bool isWhitePlayer = player == gameContext_.getWhite();
                handleEngineDisconnect(player, isWhitePlayer);
            }
        }
        eventQueue_.pop();
    }
}

void GameManager::processEvent(const EngineEvent& event) {
    try {
		PlayerContext* player = gameContext_.findPlayerByEngineId(event.engineIdentifier);
		bool isWhitePlayer = player == gameContext_.getWhite();

        if (player == nullptr) {
            // Usally from an engine in termination process. E.g. we stop an engine not reacting and already
            // Started new engines but the old engine still sends data.
            return;
        }

        // Error reporting
		std::string name = player->getEngine()->getConfig().getName();
		EngineReport* checklist = EngineReport::getChecklist(name);
        for (const auto& error : event.errors) {
            checklist->logReport(error.name, false, error.detail, error.level);
        }


        if (event.type == EngineEvent::Type::EngineDisconnected) {
            handleEngineDisconnect(player, isWhitePlayer);
            if (managerState_ != ManagerState::PlayGame) {
                finalizeTaskAndContinue();
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
            player->setComputingMove();
            return;
        }

        if (event.type == EngineEvent::Type::BestMove) {
            handleBestMove(event);
            if (managerState_ == ManagerState::ComputeMove) {
                finalizeTaskAndContinue();
                return;
            }
        }

        if (event.type == EngineEvent::Type::PonderMove) {
           handlePonderMove(event);
           return;
        }

        if (event.type == EngineEvent::Type::Info) {
            informTask(event, player);
            player->handleInfo(event);
            return;
        }

        if (managerState_ == ManagerState::PlayGame) {
            if (checkForGameEnd()) {
                finalizeTaskAndContinue();
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

    if (player != nullptr) {
        move = player->handleBestMove(event);
        moveRecord = player->getCurrentMove();
    }
	if (!move.isEmpty()) {
        gameContext_.addMove(moveRecord);
        PlayerContext* opponent = player == gameContext_.getWhite() ? gameContext_.getBlack() : gameContext_.getWhite();

		if (player != opponent) {
            opponent->doMove(move);
		}
	}

}

void GameManager::handlePonderMove(const EngineEvent& event) {
    // XBoard engines send hint (ponder move) via "Hint: <move>" to tell on what move it will ponder
    PlayerContext* sendingPlayer = gameContext_.findPlayerByEngineId(event.engineIdentifier);
    if (sendingPlayer != nullptr) {
        sendingPlayer->handlePonderMove(event);
    }
}

void GameManager::informTask(const EngineEvent& event, const PlayerContext* player) {
	if (!taskProvider_) {
		return; // No task provider set, nothing to inform
	}
	if (event.type != EngineEvent::Type::Info || !event.searchInfo) {
		return; // Only interested in info events
	}
	const auto& pv = event.searchInfo->pv;
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
    if (cause != GameEndCause::Ongoing) {
        return { cause, result };
    }

	const auto& gameRecord = gameContext_.gameRecord();

	QaplaTester::AdjudicationManager::poolInstance().testAdjudicate(gameRecord);

	auto [dcause, dresult] = QaplaTester::AdjudicationManager::poolInstance().adjudicateDraw(gameRecord);
    if (dresult != GameResult::Unterminated) {
        return { dcause, dresult };
    }

	auto [rcause, rresult] = QaplaTester::AdjudicationManager::poolInstance().adjudicateResign(gameRecord);
	if (rresult != GameResult::Unterminated) {
        return { rcause, rresult };
    }

	return { GameEndCause::Ongoing, GameResult::Unterminated };
}

bool GameManager::checkForGameEnd(bool verbose) {
    // Both player should have the right result but the player not to move is still passive
    auto [cause, result] = getGameResult();

    if (result == GameResult::Unterminated) {
        return false;
    }
    gameContext_.setGameEnd(cause, result);
    if (verbose) {
    	Logger::testLogger().log("[Result: " + gameResultToPgnResult(result) + "]", TraceLevel::info);
	    Logger::testLogger().log("[Termination: " + gameEndCauseToPgnTermination(cause) + "]", TraceLevel::info);
    }

    return true;
}

void GameManager::moveNow() {
    if (gameContext_.getPlayerCount() == 0) {
        return;
    }

    const auto& gameRecord = gameContext_.gameRecord();

    if (gameRecord.isWhiteToMove()) {
        gameContext_.getWhite()->getEngine()->moveNow();
    }
    else {
        gameContext_.getBlack()->getEngine()->moveNow();
    }
}

void GameManager::computeNextMove(const std::optional<EngineEvent>& event) {
    const auto& gameRecord = gameContext_.gameRecord();
	auto* white = gameContext_.getWhite();
	auto* black = gameContext_.getBlack();
    auto [whiteTime, blackTime] = gameRecord.timeUsed();
    GoLimits goLimits = createGoLimits(
		white->getTimeControl(), black->getTimeControl(),
        gameRecord.nextMoveIndex(), whiteTime, blackTime, gameRecord.isWhiteToMove());
	if (gameRecord.isWhiteToMove()) {
        white->computeMove(gameRecord, goLimits);
        black->allowPonder(gameRecord, goLimits, event);
    } else {
		black->computeMove(gameRecord, goLimits);
        white->allowPonder(gameRecord, goLimits, event);
    }
}

void GameManager::stop() {
    enqueueEvent(EngineEvent::createStopTask());
    /*
   {
        // Ensure no new tasks are assigned
        std::scoped_lock lock(taskProviderMutex_);
        if (taskProvider_) {
            taskProvider_ = nullptr;
        }
        managerState_ = ManagerState::None;
    }
    gameContext_.cancelCompute();
    {
        std::scoped_lock lock(queueMutex_);
        clearQueueButHandleDisconnects();
    }
    tearDown("stop");
    */
}

void GameManager::executeTask(std::optional<GameTask> task) {
    if (!task) {
        tearDown("executeTask");
        return;
    }
    // The tasks contains information how to play the next game including side switch
    gameContext_.setSideSwitched(task->switchSide);
    auto& gameRecord = task->gameRecord;

    // Also sets the engines names, Switched side must be set before to have the correct names.
    setFromGameRecord(gameRecord);

    // We need to inform engines about time controls, if they are changed directly from the gui,
    // but not as part of a new game as then newGame() will inform the engines about time controls.
    bool informEngines = false;
    gameContext_.setTimeControls(
        { gameRecord.getWhiteTimeControl(), gameRecord.getBlackTimeControl() }, informEngines);

    managerState_ = static_cast<ManagerState>(task->taskType);
    taskId_ = task->taskId;

    // Notify engines that a new game or task is starting to allow reset of internal state (e.g., memory, hash tables)
    gameContext_.newGame();
    computeNextMove();
}

std::optional<GameTask> GameManager::assignNewProviderAndTask() {
    if (pool_ == nullptr) {
        return std::nullopt;
    }
    auto extendedTask = pool_->tryAssignNewTask();
    if (!extendedTask) {
        return std::nullopt;
    }

    taskProvider_ = extendedTask->provider;
    managerState_ = ManagerState::FetchNextTask;
    // Create engines from configurations
    if (extendedTask->whiteConfig && extendedTask->blackConfig) {
        auto whiteEngines = EngineWorkerFactory::createEngines(*extendedTask->whiteConfig, 1);
        auto blackEngines = EngineWorkerFactory::createEngines(*extendedTask->blackConfig, 1);

        if (whiteEngines.empty() || blackEngines.empty()) {
            throw std::runtime_error("Failed to create engines for task assignment");
        }

        initEngines(
            std::move(whiteEngines.front()),
            std::move(blackEngines.front()));
    }
    else if (extendedTask->whiteConfig) {
        auto engines = EngineWorkerFactory::createEngines(*extendedTask->whiteConfig, 1);
        if (engines.empty()) {
            throw std::runtime_error("Failed to create engine for task assignment");
        }
        initUniqueEngine(std::move(engines.front()));
    }

    return extendedTask->task;
}

std::optional<GameTask> GameManager::nextAssignment() {
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
    try {
        if (pool_ == nullptr || !taskProvider_ || pool_->maybeDeactivateManager(taskProvider_)) {
            return std::nullopt;
        }

        std::optional<GameTask> task = taskProvider_->nextTask();
        if (task) {
            gameContext_.restartIfConfigured();
            return task;
        }

        // assignNewProviderAndTask already provides new engine instances so restarting is not needed.
        return assignNewProviderAndTask();
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception in GameManager::nextAssignment " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception in GameManager::nextAssignment", TraceLevel::error);
    }
    return std::nullopt;
}

void GameManager::finalizeTaskAndContinue() {
    if (!isRunning()) {
        // Already processed to end
        return;
    }
    
    managerState_ = ManagerState::FetchNextTask;
    
    // Cancel any ongoing compute operations
	gameContext_.cancelCompute();

    // The current game is finished - we ignore all remaining engine events except disconnects
    {
        std::scoped_lock lock(queueMutex_);
        clearQueueButHandleDisconnects();
    }
    // Inform the task provider about the finished game
	const auto& gameRecord = gameContext_.gameRecord();
    taskProvider_->setGameRecord(taskId_, gameRecord);
	QaplaTester::AdjudicationManager::poolInstance().onGameFinished(gameRecord);
    // Check if we are requested to pause
    {
        std::scoped_lock lock(pauseMutex_);
        if (pauseRequested_) {
            paused_ = true;
            return;
        }
    }

    auto task = nextAssignment();
	executeTask(std::move(task));
}

void GameManager::start(std::shared_ptr<GameTaskProvider> taskProvider) {
    // We start by playing an event to the queue to decouple any access to engines from 
    // the calling thread.
    // This also ensures, that any engine thread is child of the game-manager thread (important for linux).
    ManagerState expected = ManagerState::NotRunning;
    if (!managerState_.compare_exchange_strong(expected, ManagerState::FetchNextTask)) {
        return; // Already running
    }
    initializeFinishedFuture();
    // We do not need a lock for taskProvider_ because compare_exchange ensures 
    // only one thread proceeds past this point and managerState (NotRunning) transition 
    // prevents any concurrent access.
    taskProvider_ = std::move(taskProvider);
    enqueueEvent(EngineEvent::createStartTask());
}

void GameManager::resume() {
    // Resume from paused state. The status is still "running" and we have a taskprovider_
    {
        std::scoped_lock lock(pauseMutex_);
        pauseRequested_ = false;
        if (!paused_) {
            return;
        }
        paused_ = false;
    }
    managerState_ = ManagerState::FetchNextTask;
    enqueueEvent(EngineEvent::createStartTask());
}

} // namespace QaplaTester
