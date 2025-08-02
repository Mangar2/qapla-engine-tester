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

#include "compute-task.h"

ComputeTask::ComputeTask() {
    finishedFuture_ = finishedPromise_.get_future();
    finishedPromiseValid_ = true;
    eventThread_ = std::thread(&ComputeTask::processQueue, this);
    gameContext_.setEventCallback([this](EngineEvent&& event) {
        enqueueEvent(std::move(event));
		});
}

ComputeTask::~ComputeTask() {
    stopThread_ = true;
    queueCondition_.notify_all();
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
}

void ComputeTask::computeMove() {
    if (gameContext_.getPlayerCount() == 0) return;
    if (checkGameOver()) return;
    logMoves_ = false;
    markRunning();

    auto& gameRecord = gameContext_.gameRecord();
    auto white = gameContext_.getWhite();
    auto black = gameContext_.getBlack();

    auto [whiteTime, blackTime] = gameRecord.timeUsed();
    GoLimits goLimits = createGoLimits(
        white->getTimeControl(), black->getTimeControl(),
        gameRecord.nextMoveIndex(), whiteTime, blackTime, gameRecord.isWhiteToMove());

    if (gameRecord.isWhiteToMove()) {
        white->computeMove(gameRecord, goLimits);
    }
    else {
        black->computeMove(gameRecord, goLimits);
    }
}

void ComputeTask::autoPlay(bool logMoves) {
    logMoves_ = logMoves;
    autoPlay(std::nullopt);
}

void ComputeTask::autoPlay(const std::optional<EngineEvent>& event) {

    if (gameContext_.getPlayerCount() == 0) return;
    if (checkGameOver()) return;
    markRunning();
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
    taskType_ = ComputeTaskType::Autoplay;
}

void ComputeTask::moveNow() {
    if (gameContext_.getPlayerCount() == 0) return;

    auto& gameRecord = gameContext_.gameRecord();

    if (gameRecord.isWhiteToMove()) {
        gameContext_.getWhite()->getEngine()->moveNow();
    }
    else {
        gameContext_.getBlack()->getEngine()->moveNow();
    }
}

const std::future<void>& ComputeTask::getFinishedFuture() const {
    return finishedFuture_;
}

void ComputeTask::stop() {
    taskType_ = ComputeTaskType::None;
    gameContext_.cancelCompute();
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!eventQueue_.empty()) {
            eventQueue_.pop();
        }
    }

    markFinished();
}


void ComputeTask::enqueueEvent(const EngineEvent& event) {
    if (event.type == EngineEvent::Type::None || event.type == EngineEvent::Type::NoData) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.push(event);
    }
    queueCondition_.notify_one();
}

void ComputeTask::processQueue() {
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

        while (true) {
            EngineEvent event;
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                if (eventQueue_.empty()) break;
                event = std::move(eventQueue_.front());
                eventQueue_.pop();
            }
            processEvent(event);
        }

        if (std::chrono::steady_clock::now() >= nextTimeoutCheck) {
            nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;

            bool restarted = gameContext_.checkForTimeoutsAndRestart();
            if (restarted) markFinished();
        }
    }
}

void ComputeTask::processEvent(const EngineEvent & event) {

    PlayerContext* player = gameContext_.findPlayerByEngineId(event.engineIdentifier);
    if (!player) return;

    if (!event.errors.empty()) {
        const std::string& name = player->getEngine()->getConfig().getName();
        EngineReport* checklist = EngineReport::getChecklist(name);
        for (const auto& error : event.errors) {
            checklist->logReport(error.name, false, error.detail, error.level);
        }
    }

    if (event.type == EngineEvent::Type::EngineDisconnected) {
        player->handleDisconnect(true);
        player->getEngine()->setEventSink([this](EngineEvent&& e) {
            enqueueEvent(std::move(e));
            });
        return;
    }

    if (event.type == EngineEvent::Type::ComputeMoveSent) {
        player->setComputeMoveStartTimestamp(event.timestampMs);
        return;
    }

    if (event.type == EngineEvent::Type::SendingComputeMove) {
        player->setComputingMove();
        return;
    }

    if (event.type == EngineEvent::Type::BestMove) {
        handleBestMove(event);
        nextMove(event);
        return;
    }

    if (event.type == EngineEvent::Type::Info) {
        player->handleInfo(event);
    }
}

void ComputeTask::handleBestMove(const EngineEvent& event) {
    QaplaBasics::Move move;
    MoveRecord moveRecord;

    PlayerContext* player = gameContext_.findPlayerByEngineId(event.engineIdentifier);
    if (!player) return;

    if (logMoves_ && event.bestMove) {
        std::cout << *event.bestMove << " " << std::flush;
    }

    move = player->handleBestMove(event);
    moveRecord = player->getCurrentMove();

    if (!move.isEmpty()) {
        auto& gameRecord = gameContext_.gameRecord();
        gameRecord.addMove(moveRecord);

        PlayerContext* opponent =
            (player == gameContext_.getWhite()) ? gameContext_.getBlack() : gameContext_.getWhite();

        if (opponent != player) {
            opponent->doMove(move);
        }
    }
}

void ComputeTask::nextMove(const EngineEvent& event) {
    if (taskType_ != ComputeTaskType::Autoplay) {
        markFinished();
        return;
    }

    if (checkGameOver(true)) {
        markFinished();
    }
    else {
        autoPlay(event);
    }
}


bool ComputeTask::checkGameOver(bool verbose) {
    auto [cause, result] = gameContext_.checkGameResult();

    if (result == GameResult::Unterminated) {
        return false;
    }

    if (verbose) {
        if (logMoves_) std::cout << "\n";
        Logger::testLogger().log("[Result: " + gameResultToPgnResult(result) + "]", TraceLevel::info);
        Logger::testLogger().log("[Termination: " + gameEndCauseToPgnTermination(cause) + "]", TraceLevel::info);
    }

    return true;
}

void ComputeTask::markFinished() {
    if (finishedPromiseValid_) {
        try {
            finishedPromise_.set_value();
        }
        catch (const std::future_error&) {
            // already satisfied
        }
        finishedPromiseValid_ = false;
    }
    taskType_ = ComputeTaskType::None;
}

void ComputeTask::markRunning() {
    if (!finishedPromiseValid_) {
        finishedPromise_ = std::promise<void>();
        finishedFuture_ = finishedPromise_.get_future();
        finishedPromiseValid_ = true;
    }
}




