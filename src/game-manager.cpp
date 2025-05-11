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

GameManager::GameManager(std::unique_ptr<EngineWorker> engine)
    : engine_(std::move(engine)) {

    engine_->setEventSink([this](const EngineEvent& event) {
        handleState(event);
     });
}

void GameManager::handleState(const EngineEvent& event) {
    if (event.type == EngineEvent::Type::BestMove) {
        handleBestMove(event);
        //finishedPromise_.set_value();
    }
    else if (event.type == EngineEvent::Type::ComputeMoveSent) {
		computeMoveStartTimestamp_ = event.timestampMs;
    }
}

void GameManager::handleBestMove(const EngineEvent& event) {
    if (!handleCheck("Computing a move did not return a move", !event.bestMove.has_value())) return;
	const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
	if (!handleCheck("Computing a move returned an illegal move", move.isEmpty(), *event.bestMove)) return;
    gameState_.doMove(move);
    checkTime(event);
    computeMove();
}

void GameManager::checkTime(const EngineEvent& event) {
	const int64_t GRACE_MS = 5; // ms
    const int64_t elapsedMs = event.timestampMs - computeMoveStartTimestamp_;
    const GoLimits& limits = timeControl_.createGoLimits();
    const bool white = gameState_.isWhiteToMove();
    int numLimits = 0;

     // Overrun ist always an error, every limit must be respected
    const int64_t timeLeft = white ? limits.wtimeMs : limits.btimeMs;
    if (timeLeft > 0) {
		handleCheck("wtime/btime overrun", elapsedMs >= timeLeft,
			std::to_string(elapsedMs) + " >= " + std::to_string(timeLeft));
        numLimits++;
    }

	if (limits.movetimeMs.has_value()) {
		handleCheck("movetime overrun", elapsedMs > *limits.movetimeMs + GRACE_MS,
			std::to_string(elapsedMs) + " >= " + std::to_string(*limits.movetimeMs));
        numLimits++;
	}

    // Underrun is never an error; we still check it for selected time models
    // to verify if the engine follows expected timing behavior.
    // This check is only applied if exactly one time constraint is active.
    if (numLimits == 1) {
        if (limits.movetimeMs.has_value()) {
            handleCheck("movetime underrun", elapsedMs < *limits.movetimeMs * 9 / 10,
                std::to_string(elapsedMs) + " < " + std::to_string(*limits.movetimeMs));
        }
    }
}

void GameManager::computeMove() {
    GoLimits limits = timeControl_.createGoLimits();
    engine_->computeMove(gameState_, limits);
}

bool GameManager::isLegalMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    return !move.isEmpty();
}

void GameManager::runTests() {
    // Movetime support
	timeControl_.setMoveTime(1000);
    computeMove();
}

void GameManager::run() {
    runTests();
}