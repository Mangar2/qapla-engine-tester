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
}

ComputeTask::~ComputeTask() {
    stopThread_ = true;
    queueCondition_.notify_all();
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
}

void ComputeTask::initEngines(std::vector<std::unique_ptr<EngineWorker>> engines) {
    players_.clear();
    for (auto& engine : engines) {
        engine->setEventSink([this](EngineEvent&& event) {
            enqueueEvent(std::move(event));
            });

        auto player = std::make_unique<PlayerContext>();
        player->setEngine(std::move(engine));
        players_.emplace_back(std::move(player));
    }
}

void ComputeTask::restartEngine(uint32_t index) {
    if (index >= players_.size()) return;

    players_[index]->restartEngine();
    players_[index]->getEngine()->setEventSink([this](EngineEvent&& event) {
        enqueueEvent(std::move(event));
        });
}

void ComputeTask::setTimeControl(const TimeControl& timeControl) {
    for (auto& player : players_) {
        player->setTimeControl(timeControl);
    }
}

void ComputeTask::newGame() {
    for (size_t i = 0; i < players_.size(); ++i) {
        bool isWhite = (i == 0);
        players_[i]->newGame(gameRecord_, isWhite);
    }
}

void ComputeTask::setPosition(bool useStartPosition, const std::string& fen,
    std::optional<std::vector<std::string>> playedMoves) {

    gameRecord_.setStartPosition(useStartPosition, fen, true,
        players_.empty() ? "" : players_[0]->getEngine()->getConfig().getName(),
        players_.size() > 1 ? players_[1]->getEngine()->getConfig().getName() : "");

    if (playedMoves) {
        for (const auto& move : *playedMoves) {
            gameRecord_.addMove({ .original = move, .lan = move });
        }
    }

    for (auto& player : players_) {
        player->setStartPosition(gameRecord_);
    }
}

void ComputeTask::computeMove(std::optional<uint32_t> index) {
    if (players_.empty()) return;
    if (index && *index >= players_.size()) {
        return;
	}
    markRunning();
    GoLimits goLimits = createGoLimits(
        players_.front()->getTimeControl(), players_.front()->getTimeControl(),
        gameRecord_.nextMoveIndex(), 0, 0, gameRecord_.isWhiteToMove());

    if (index) {
        players_[*index]->computeMove(gameRecord_, goLimits);
    }
    else {
        for (auto& player : players_) {
            player->computeMove(gameRecord_, goLimits);
		}
    }
}

void ComputeTask::autoplay() {
    if (players_.size() < 2) return;
    markRunning();

    auto [whiteTime, blackTime] = gameRecord_.timeUsed();
    GoLimits goLimits = createGoLimits(
        players_[0]->getTimeControl(), players_[1]->getTimeControl(),
        gameRecord_.nextMoveIndex(), whiteTime, blackTime, gameRecord_.isWhiteToMove());

    if (gameRecord_.isWhiteToMove()) {
        players_[0]->computeMove(gameRecord_, goLimits);
        players_[1]->allowPonder(gameRecord_, goLimits);
    }
    else {
        players_[1]->computeMove(gameRecord_, goLimits);
        players_[0]->allowPonder(gameRecord_, goLimits);
    }
}

void ComputeTask::moveNow() {
    if (players_.empty()) return;

    if (players_.size() == 1 || gameRecord_.isWhiteToMove()) {
        players_[0]->getEngine()->moveNow();
    }
    else {
        players_[1]->getEngine()->moveNow();
    }
}

const std::future<void>& ComputeTask::getFinishedFuture() const {
    return finishedFuture_;
}

void ComputeTask::stop() {
    stopThread_ = true;
    queueCondition_.notify_all();
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

            bool restarted = false;
            for (auto& player : players_) {
                if (player->checkEngineTimeout(false)) {
                    restarted = true;
                    player->getEngine()->setEventSink([this](EngineEvent&& event) {
                        enqueueEvent(std::move(event));
                        });
                }
            }

            if (checkGameOver() || (restarted && players_.size() == 1)) {
                markFinished();
            }
            else if (restarted && players_.size() > 1) {
                autoplay();
            }
        }
    }
}


void ComputeTask::processEvent(const EngineEvent& event) {
    for (auto& player : players_) {
        if (player->getEngine()->getIdentifier() != event.engineIdentifier) {
            continue;
        }

        if (!event.errors.empty()) {
            std::string name = player->getEngine()->getConfig().getName();
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
            player->setComputingMove(true);
            return;
        }

        if (event.type == EngineEvent::Type::BestMove) {
            handleBestMove(event);
            if (players_.size() == 1) {
                markFinished();
            }
            else {
                autoplay(); // continue game
            }
            return;
        }

        if (event.type == EngineEvent::Type::Info) {
            player->handleInfo(event);
        }

        break;
    }
}

void ComputeTask::handleBestMove(const EngineEvent& event) {
    QaplaBasics::Move move;
    MoveRecord moveRecord;
    PlayerContext* player = nullptr;
    PlayerContext* opponent = nullptr;

    if (logMoves_) std::cout << *event.bestMove << " " << std::flush;

    if (players_.size() == 1 || players_[0]->getEngine()->getIdentifier() == event.engineIdentifier) {
        player = players_[0].get();
        if (players_.size() > 1) opponent = players_[1].get();
    }
    else if (players_.size() > 1 && players_[1]->getEngine()->getIdentifier() == event.engineIdentifier) {
        player = players_[1].get();
        opponent = players_[0].get();
    }

    if (player) {
        move = player->handleBestMove(event);
        moveRecord = player->getCurrentMove();
    }

    if (move != QaplaBasics::Move::EMPTY_MOVE) {
        gameRecord_.addMove(moveRecord);
        if (opponent && opponent != player) {
            opponent->doMove(move);
        }
    }
}

std::tuple<GameEndCause, GameResult> ComputeTask::getGameResult() {
    GameEndCause cause = GameEndCause::Ongoing;
    GameResult result = GameResult::Unterminated;

    for (auto& player : players_) {
        auto [pcause, presult] = player->getGameResult();
        if (pcause != GameEndCause::Ongoing) {
            return { pcause, presult };
        }
    }

    return { GameEndCause::Ongoing, GameResult::Unterminated };
}

bool ComputeTask::checkGameOver() {
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
}

void ComputeTask::markRunning() {
    if (!finishedPromiseValid_) {
        finishedPromise_ = std::promise<void>();
        finishedFuture_ = finishedPromise_.get_future();
        finishedPromiseValid_ = true;
    }
}




