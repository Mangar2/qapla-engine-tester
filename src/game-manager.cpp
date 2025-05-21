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
#include "checklist.h"
#include <iostream>

GameManager::GameManager(): taskProvider_(nullptr) {
    heartBeat_ = std::make_unique<HeartBeat>([this]() {
        handleState(EngineEvent::create(EngineEvent::Type::KeepAlive, "", Timer::getCurrentTimeMs()));
        });
}

void GameManager::setUniqueEngine(std::shared_ptr<EngineWorker> engine) {
    whitePlayer_.setEngine(engine, requireLan_);
	blackPlayer_.setEngine(engine, requireLan_);

    engine->setEventSink([this](const EngineEvent& event) {
        handleState(event);
        });
}

void GameManager::setEngines(std::shared_ptr<EngineWorker> white, std::shared_ptr<EngineWorker> black) {

    white->setEventSink([this](const EngineEvent& event) {
        handleState(event);
        });

    if (black != white) {
        black->setEventSink([this](const EngineEvent& event) {
            handleState(event);
            });
    }
    whitePlayer_.setEngine(white, requireLan_);
    blackPlayer_.setEngine(black, requireLan_);
}

void GameManager::switchSide() {
    std::swap(whitePlayer_, blackPlayer_);
}

void GameManager::markFinished() {
    if (finishedPromiseValid_) {
        try {
            finishedPromise_.set_value();
        }
        catch (const std::future_error&) {
            // already satisfied – ignore or log
        }
        finishedPromiseValid_ = false;
    }
}

void GameManager::handleState(const EngineEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    for (auto& error: event.errors) {
        Checklist::logCheck(error.name, false, error.detail);
    }
    if (event.type == EngineEvent::Type::KeepAlive) {
        return;
    }
	if (event.engineIdentifier != whitePlayer_.getEngine()->getIdentifier() &&
		event.engineIdentifier != blackPlayer_.getEngine()->getIdentifier()) {
        // Usally from an engine in termination process. E.g. we stop an engine not reacting and already
        // Started new engines but the old engine still sends data.
        return;
	}
    if (event.type == EngineEvent::Type::BestMove) {
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
                computeNextMove();
            }
        } 
        else if (task_ == Tasks::ParticipateInTournament) {
            if (checkForGameEnd()) {
                computeNextGame();
            }
			else {
				computeNextMove();
			}
        }
    }
    else if (event.type == EngineEvent::Type::Info) {
        handleInfo(event);
    }
    else if (event.type == EngineEvent::Type::ComputeMoveSent) {
        // We get the start calculating move timestamp directly from the EngineProcess after sending the compute move string
		// to the engine. This prevents loosing time for own synchronization tasks on the engines clock.
		if (event.engineIdentifier == whitePlayer_.getEngine()->getIdentifier()) {
			whitePlayer_.setComputeMoveStartTimestamp(event.timestampMs);
		}
		else if (event.engineIdentifier == blackPlayer_.getEngine()->getIdentifier()) {
			blackPlayer_.setComputeMoveStartTimestamp(event.timestampMs);
		}
    }
}

void GameManager::handleHeartBeat() {

}

void GameManager::handleBestMove(const EngineEvent& event) {
    QaplaBasics::Move move;
	MoveRecord moveRecord;
	PlayerContext* playerToInform = nullptr;
    if (logMoves_) std::cout << *event.bestMove << " " << std::flush;
	if (whitePlayer_.getEngine()->getIdentifier() == event.engineIdentifier) {
        move = whitePlayer_.handleBestMove(event);
		moveRecord = whitePlayer_.getCurrentMove();
        playerToInform = &blackPlayer_;
	}
	else if (blackPlayer_.getEngine()->getIdentifier() == event.engineIdentifier) {
		move = blackPlayer_.handleBestMove(event);
		moveRecord = blackPlayer_.getCurrentMove();
        playerToInform = &whitePlayer_;
	}
	if (move != QaplaBasics::Move::EMPTY_MOVE) {
		gameRecord_.addMove(moveRecord);
		if (playerToInform->getEngine()->getIdentifier() != event.engineIdentifier) {
            playerToInform->doMove(move);
		}
	}

}

void GameManager::handleInfo(const EngineEvent& event) {
	if (!event.searchInfo.has_value()) return;
	if (whitePlayer_.getEngine()->getIdentifier() == event.engineIdentifier) {
        whitePlayer_.handleInfo(event);
	}
	else if (blackPlayer_.getEngine()->getIdentifier() == event.engineIdentifier) {
		blackPlayer_.handleInfo(event);
	}
}

bool GameManager::checkForGameEnd() {
    // Both player should have the right result but the player not to move is still passive
	auto [cause, result] = gameRecord_.isWhiteToMove() ? blackPlayer_.getGameResult() : whitePlayer_.getGameResult();
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
		whitePlayer_.getEngine()->moveNow();
    }
    else {
		blackPlayer_.getEngine()->moveNow();
    }
}

void GameManager::computeMove(bool useStartPosition, const std::string fen) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
	newGame(useStartPosition, fen);
	gameRecord_.setTimeControl(whitePlayer_.getTimeControl(), blackPlayer_.getTimeControl());
	task_ = Tasks::ComputeMove;
    logMoves_ = false;
    computeNextMove();
}

void GameManager::computeNextMove() {
    auto [whiteTime, blackTime] = gameRecord_.timeUsed();
    GoLimits goLimits = createGoLimits(
		whitePlayer_.getTimeControl(), blackPlayer_.getTimeControl(),
        gameRecord_.currentPly(), whiteTime, blackTime, gameRecord_.isWhiteToMove());
	if (gameRecord_.isWhiteToMove()) {
        whitePlayer_.computeMove(gameRecord_, goLimits);
    }
    else {
		blackPlayer_.computeMove(gameRecord_, goLimits);
    }
}

void GameManager::computeGame(bool useStartPosition, const std::string fen, bool logMoves) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
	newGame(useStartPosition, fen);
    task_ = Tasks::PlayGame;
	logMoves_ = logMoves;
    computeNextMove();
}

void GameManager::computeNextGame() {
	if (!taskProvider_) {
		finishedPromise_.set_value();
		return;
	}
    if (gameRecord_.currentPly() > 0) {
        taskProvider_->setGameRecord(gameRecord_);
    }
    auto newTask = taskProvider_->nextTask();
	if (!newTask) {
		finishedPromise_.set_value();
		return;
	}
	auto task = *newTask;
	newGame(task.useStartPosition, task.fen);
	gameRecord_.setTimeControl(task.whiteTimeControl, task.blackTimeControl);
	setTimeControls(task.whiteTimeControl, task.blackTimeControl);
    computeNextMove();
}

void GameManager::computeGames(GameTaskProvider* taskProvider) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
    task_ = Tasks::ParticipateInTournament;
	taskProvider_ = std::move(taskProvider);
    logMoves_ = false;
    computeNextGame();
}

void GameManager::run() {
}