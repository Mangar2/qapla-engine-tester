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
#include <iostream>

GameManager::GameManager(std::unique_ptr<EngineWorker> engine)
    : engine_(std::move(engine)) {

    engine_->setEventSink([this](const EngineEvent& event) {
        if (event.type == EngineEvent::Type::BestMove) {
            std::cout << "Best move: " << (event.bestMove ? *event.bestMove : "(none)") << "\n";
        }
        });
}

void GameManager::start() {
    GameState game;
    GoLimits limits;
    limits.movetimeMs = 1000;

    engine_->computeMove(game, limits);
}