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

void GameManager::markFinished() {
    // Verhindert Ausnahme bei mehrfacher set_value()
    if (finishedPromiseValid_) {
        try {
            finishedPromise_.set_value();
        }
        catch (const std::future_error&) {
            // already satisfied – ignorieren oder loggen
        }
        finishedPromiseValid_ = false;
    }
}

void GameManager::handleState(const EngineEvent& event) {
    for (auto& error: event.errors) {
        handleCheck(error.name, false, error.detail);
    }
    if (event.type == EngineEvent::Type::BestMove) {
        handleBestMove(event);
        if (task_ == Tasks::ComputeMove) {
            finishedPromise_.set_value();
			task_ = Tasks::None;
        }
    }
    else if (event.type == EngineEvent::Type::Info) {
        handleInfo(event);
    }
    else if (event.type == EngineEvent::Type::ComputeMoveSent) {
		computeMoveStartTimestamp_ = event.timestampMs;
    }
}

void GameManager::handleBestMove(const EngineEvent& event) {
    if (!handleCheck("Computing a move returns a move check", event.bestMove.has_value())) return;
	const auto move = gameState_.stringToMove(*event.bestMove, requireLan_);
	if (!handleCheck("Computing a move returns a legal move check", !move.isEmpty(), *event.bestMove)) return;
    gameState_.doMove(move);
    checkTime(event);
}

void GameManager::handleInfo(const EngineEvent& event) {
	if (!event.searchInfo.has_value()) return;
    const auto& searchInfo = *event.searchInfo;

    // Prüfe currMove, falls vorhanden
    if (searchInfo.currMove) {
        const auto move = gameState_.stringToMove(*searchInfo.currMove, requireLan_);
        handleCheck("currmove check", !move.isEmpty(),
            "Encountered illegal move " + *searchInfo.currMove + " in currMove, raw info line " + event.rawLine);
    }

    // Prüfe PV, falls vorhanden
    if (!searchInfo.pv.empty()) {
        std::vector<QaplaBasics::Move> pvMoves;
        pvMoves.reserve(searchInfo.pv.size());

        bool valid = true;
        for (size_t i = 0; i < searchInfo.pv.size(); ++i) {
            const auto& moveStr = searchInfo.pv[i];
            const auto move = gameState_.stringToMove(moveStr, requireLan_);
            if (move.isEmpty()) {
                std::string fullPv;
                for (const auto& m : searchInfo.pv)
                    fullPv += m + " ";
                if (!fullPv.empty()) fullPv.pop_back(); // letztes Leerzeichen entfernen
                handleCheck("PV check", true,
                    "Encountered illegal move " + moveStr + " in pv " + fullPv);
                valid = false;
                break;
            }
            gameState_.doMove(move);
            pvMoves.push_back(move);
        }

        for (size_t i = 0; i < pvMoves.size(); ++i)
            gameState_.undoMove();

        if (valid)
            handleCheck("PV check", true); 
    }
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
		handleCheck("wtime/btime overrun check", elapsedMs < timeLeft,
			std::to_string(elapsedMs) + " < " + std::to_string(timeLeft));
        numLimits++;
    }

	if (limits.movetimeMs.has_value()) {
		handleCheck("movetime overrun check", elapsedMs < *limits.movetimeMs + GRACE_MS,
			std::to_string(elapsedMs) + " < " + std::to_string(*limits.movetimeMs));
        numLimits++;
	}

    // Underrun is never an error; we still check it for selected time models
    // to verify if the engine follows expected timing behavior.
    // This check is only applied if exactly one time constraint is active.
    if (numLimits == 1) {
        if (limits.movetimeMs.has_value()) {
            handleCheck("movetime underrun check", elapsedMs > *limits.movetimeMs * 9 / 10,
                std::to_string(elapsedMs) + " > " + std::to_string(*limits.movetimeMs));
        }
    }
}

void GameManager::computeMove(bool startPos, const std::string fen) {
    finishedPromise_ = std::promise<void>{};
    finishedFuture_ = finishedPromise_.get_future();
	gameState_.setFen(startPos, fen);
	task_ = Tasks::ComputeMove;
    computeMove();
}

void GameManager::computeMove() {
    GoLimits limits = timeControl_.createGoLimits();
    engine_->computeMove(gameState_, limits);
}

bool GameManager::isLegalMove(const std::string& moveText) {
    const auto move = gameState_.stringToMove(moveText, requireLan_);
    return !move.isEmpty();
}

void GameManager::run() {
}