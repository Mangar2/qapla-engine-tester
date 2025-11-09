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

namespace QaplaTester {

ComputeTask::ComputeTask()
{
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

void ComputeTask::setPosition(bool useStartPosition, const std::string &fen,
                    const std::optional<std::vector<std::string>>& playedMoves)
{
    auto taskType = taskType_;
    stop();
    gameContext_.setPosition(useStartPosition, fen, playedMoves);
    if (taskType == ComputeTaskType::Analyze) {
        analyze();
    }
}

void ComputeTask::setPosition(const GameRecord &game)
{
    auto taskType = taskType_;
    stop();
    gameContext_.setPosition(game);
    if (taskType == ComputeTaskType::Analyze) {
        analyze();
    }
}

void ComputeTask::setNextMoveIndex(uint32_t moveIndex)
{
    auto taskType = taskType_;
    stop();
    gameContext_.setNextMoveIndex(moveIndex);
    if (taskType == ComputeTaskType::Analyze) {
        analyze();
    }
}

void ComputeTask::ponderMove(const std::optional<EngineEvent>& event) {
    if (gameContext_.getPlayerCount() == 0) {
        return;
    }
    if (checkGameOver()) {
        return;
    }
    if (taskType_ != ComputeTaskType::None && taskType_ != ComputeTaskType::PlaySide) {
        return;
    }
    gameContext_.ensureStarted();
    logMoves_ = false;
    taskType_ = taskType_ == ComputeTaskType::None ? ComputeTaskType::ComputeMove : taskType_;
    markRunning();

    const auto& gameRecord = gameContext_.gameRecord();
    auto* white = gameContext_.getWhite();
    auto* black = gameContext_.getBlack();

    auto [whiteTime, blackTime] = gameRecord.timeUsed();
    GoLimits goLimits = createGoLimits(
        white->getTimeControl(), black->getTimeControl(),
        gameRecord.nextMoveIndex(), whiteTime, blackTime, gameRecord.isWhiteToMove());

    if (gameRecord.isWhiteToMove()) {
        black->allowPonder(gameRecord, goLimits, event);
    }
    else {
        white->allowPonder(gameRecord, goLimits, event);
    }
}

void ComputeTask::computeMove() {
    if (gameContext_.getPlayerCount() == 0) {
        return;
    }
    if (checkGameOver()) {
        return;
    }
    if (taskType_ != ComputeTaskType::None && taskType_ != ComputeTaskType::PlaySide) {
        return;
    }
    gameContext_.ensureStarted();
    logMoves_ = false;
    taskType_ = taskType_ == ComputeTaskType::None ? ComputeTaskType::ComputeMove : taskType_;
    markRunning();

    const auto& gameRecord = gameContext_.gameRecord();
    auto* white = gameContext_.getWhite();
    auto* black = gameContext_.getBlack();

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

void ComputeTask::analyze() {
    if (gameContext_.getPlayerCount() == 0) { return; }
    if (checkGameOver()) { return; }
    if (taskType_ != ComputeTaskType::None) { return; }
    gameContext_.ensureStarted();
    logMoves_ = false;
    taskType_ = ComputeTaskType::Analyze;
    markRunning();
    const auto& gameRecord = gameContext_.gameRecord();
    TimeControl time;
    GoLimits goLimits;
    goLimits.hasTimeControl = true;
	goLimits.infinite = true;
    for (size_t i = 0; i < gameContext_.getPlayerCount(); ++i) {
        auto* player = gameContext_.player(i);
        if (player != nullptr) {
            player->computeMove(gameRecord, goLimits, true);
        }
	}
}

void ComputeTask::autoPlay(bool logMoves) {
    logMoves_ = logMoves;
    if (taskType_ != ComputeTaskType::None) {
        return;
    }
    taskType_ = ComputeTaskType::Autoplay;
    autoPlay(std::nullopt);
}

void ComputeTask::autoPlay(const std::optional<EngineEvent>& event) {
    if (gameContext_.getPlayerCount() == 0) { return; }
    if (checkGameOver()) { return; }
    if (taskType_ != ComputeTaskType::Autoplay ) { return; }
    markRunning();
    gameContext_.ensureStarted();
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
    }
    else {
        black->computeMove(gameRecord, goLimits);
        white->allowPonder(gameRecord, goLimits, event);
    }
}

void ComputeTask::moveNow() {
    if (gameContext_.getPlayerCount() == 0) {
        return;
    }

    const auto& gameRecord = gameContext_.gameRecord();
    auto* player = gameRecord.isWhiteToMove() ? gameContext_.getWhite() : gameContext_.getBlack();
    if (player == nullptr) {
        return;
    }
    // Check Ready is ok in UCI, but not in Winboard mode, I do not know why it is here and if it is needed for uci.
    // player->checkReady();
    player->moveNow();
}

const std::future<void>& ComputeTask::getFinishedFuture() const {
    return finishedFuture_;
}

void ComputeTask::stop() {
    taskType_ = ComputeTaskType::None;
    gameContext_.cancelCompute();
    {
        std::scoped_lock lock(queueMutex_);
        while (!eventQueue_.empty()) {
            eventQueue_.pop();
        }
    }

    markFinished();
}

void ComputeTask::enqueueEvent(EngineEvent&& event) {
    if (event.type == EngineEvent::Type::None || event.type == EngineEvent::Type::NoData) {
        return;
    }
    {
        std::scoped_lock lock(queueMutex_);
        eventQueue_.push(std::move(event));
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
            if (taskType_ == ComputeTaskType::Analyze) {
                queueCondition_.wait(lock, [this] {
                    return !eventQueue_.empty() || stopThread_;
                    });
            }
            else {
                queueCondition_.wait_until(lock, nextTimeoutCheck, [this] {
                    return !eventQueue_.empty() || stopThread_;
                    });
            }
        }

        while (true) {
            EngineEvent event;
            {
                std::scoped_lock lock(queueMutex_);
                if (eventQueue_.empty()) {
                    break;
                }
                event = std::move(eventQueue_.front());
                eventQueue_.pop();
            }
            processEvent(event);
        }

        if (taskType_ == ComputeTaskType::Analyze) {
            continue;
        }

        if (std::chrono::steady_clock::now() >= nextTimeoutCheck) {
            nextTimeoutCheck = std::chrono::steady_clock::now() + timeoutInterval;

            bool restarted = gameContext_.checkForTimeoutsAndRestart();
            if (restarted) {
                markFinished();
            }
        }
    }
}

void ComputeTask::processEvent(const EngineEvent & event) {

    PlayerContext* player = gameContext_.findPlayerByEngineId(event.engineIdentifier);
    if (player == nullptr) {
        return;
    }

    if (!event.errors.empty()) {
        const std::string& name = player->getEngine()->getConfig().getName();
        EngineReport* checklist = EngineReport::getChecklist(name);
        for (const auto& error : event.errors) {
            checklist->logReport(error.name, false, error.detail, error.level);
        }
    }

    if (event.type == EngineEvent::Type::EngineDisconnected) {
        player->handleDisconnect(true);
        player->getEngine()->setEventSink([this](EngineEvent&& event) {
            enqueueEvent(std::move(event));
            });
        return;
    }

    if (event.type == EngineEvent::Type::ComputeMoveSent) {
        player->setComputeMoveStartTimestamp(event.timestampMs);
        return;
    }

    if (event.type == EngineEvent::Type::SendingComputeMove) {
        // Sent from engine worker thread directly before sending a compute move command to the engine
        // We use this to ensure that all engine information received before this event are counted as messages 
        // from the previous move
        player->setComputingMove();
        return;
    }

    if (event.type == EngineEvent::Type::BestMove) {
        handleBestMove(event);
        nextMove(event);
        return;
    }

    if (event.type == EngineEvent::Type::PonderMove) {
        handlePonderMove(event);
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
    if (player == nullptr) {
        return;
    }

    if (logMoves_ && event.bestMove) {
        std::cout << *event.bestMove << " " << std::flush;
    }

    move = player->handleBestMove(event);
    moveRecord = player->getCurrentMove();

    if (!move.isEmpty()) {
        gameContext_.addMove(moveRecord);

        PlayerContext* opponent =
            (player == gameContext_.getWhite()) ? gameContext_.getBlack() : gameContext_.getWhite();

        if (opponent != player) {
            opponent->doMove(move);
        }
    }
}

void ComputeTask::handlePonderMove(const EngineEvent& event) {
    // XBoard engines send hint (ponder move) via "Hint: <move>" to tell on what move it will ponder
    PlayerContext* sendingPlayer = gameContext_.findPlayerByEngineId(event.engineIdentifier);
    if (sendingPlayer == nullptr) {
        return;
    }
    sendingPlayer->handlePonderMove(event);
}

void ComputeTask::nextMove(const EngineEvent& event) {
    if (checkGameOver(true)) {
        markFinished();
        return;
    }
    if (taskType_ == ComputeTaskType::Autoplay) {
        autoPlay(event);
        return;
    }
    if (taskType_ == ComputeTaskType::PlaySide) {
        ponderMove(event);
        return;
    }

    markFinished();
}


bool ComputeTask::checkGameOver(bool verbose) {

    auto [cause, result] = gameContext_.checkGameResult();
    if (logMoves_) {
        std::cout << "\n";
    }
    if (verbose && result != GameResult::Unterminated) {
        Logger::testLogger().log("[Result: " + gameResultToPgnResult(result) + "]", TraceLevel::info);
        Logger::testLogger().log("[Termination: " + gameEndCauseToPgnTermination(cause) + "]", TraceLevel::info);
    }

    return gameContext_.gameRecord().isGameOver();
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

} // namespace QaplaTester


