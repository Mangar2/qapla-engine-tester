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
        evaluateMovetime(event);
        finishedPromise_.set_value();
    }
}

void GameManager::evaluateMovetime(const EngineEvent& event) {
    const auto elapsedMs = timer_.elapsedMs(event.timestampMs);
    std::string resultText;
    bool passed = true;

    if (auto moveTime = timeControl_.moveTimeMs()) {
        const int64_t lower = (*moveTime * 85) / 100;
        const int64_t upper = *moveTime;

        if (elapsedMs < lower) {
            resultText = "used too little time (" + std::to_string(elapsedMs) + "ms)";
            passed = false;
        }
        else if (elapsedMs > upper) {
            resultText = "exceeded time limit (" + std::to_string(elapsedMs) + "ms)";
            passed = false;
        }
        else {
            resultText = "used " + std::to_string(elapsedMs) + "ms";
        }
    }
    else {
        resultText = "no movetime specified";
        passed = false;
    }

    EngineChecklist::addItem("Timemanagement", "Supports movetime: " + resultText, passed);
}

void GameManager::computeMove() {
    GoLimits limits = timeControl_.createGoLimits();
    timer_.start();
    engine_->computeMove(gameState_, limits);
}

void GameManager::runTests() {
    // Movetime support
	timeControl_.setMoveTime(1000);
    computeMove();
}

void GameManager::run() {
    runTests();
}