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
#include "game-context.h"

GameContext::GameContext() = default;

void GameContext::initPlayers(std::vector<std::unique_ptr<EngineWorker>> engines) {
    players_.clear();
    for (auto& engine : engines) {
        if (eventCallback_) {
            engine->setEventSink(eventCallback_);
        }
        auto player = std::make_unique<PlayerContext>();
        player->setEngine(std::move(engine));
        players_.emplace_back(std::move(player));
    }
}

void GameContext::restartPlayer(uint32_t index) {
    if (index >= players_.size()) return;
    players_[index]->restartEngine();
    if (eventCallback_) {
        players_[index]->getEngine()->setEventSink(eventCallback_);
    }
}

void GameContext::setTimeControl(const TimeControl& timeControl) {
    for (auto& player : players_) {
        player->setTimeControl(timeControl);
    }
}

void GameContext::newGame() {
    for (size_t i = 0; i < players_.size(); ++i) {
        bool isWhite = (i == 0 && !switchedSide_) || (i == 1 && switchedSide_);
        players_[i]->newGame(gameRecord_, isWhite);
    }
}

void GameContext::setPosition(bool useStartPosition, const std::string& fen,
    std::optional<std::vector<std::string>> playedMoves) {
    auto* white = getWhite();
    auto* black = getBlack();
    const std::string whiteName = white && white->getEngine() ? white->getEngine()->getConfig().getName() : "";
    const std::string blackName = black && black->getEngine() ? black->getEngine()->getConfig().getName() : "";

    gameRecord_.setStartPosition(useStartPosition, fen, true, whiteName, blackName);

    if (playedMoves) {
        for (const auto& move : *playedMoves) {
            gameRecord_.addMove({ .original = move, .lan = move });
        }
    }

    for (auto& player : players_) {
        player->setStartPosition(gameRecord_);
    }
}

void GameContext::setPosition(const GameRecord& record) {
    gameRecord_ = record;
    auto* white = getWhite();
    auto* black = getBlack();
    const std::string whiteName = white && white->getEngine() ? white->getEngine()->getConfig().getName() : "";
    const std::string blackName = black && black->getEngine() ? black->getEngine()->getConfig().getName() : "";

    gameRecord_.setWhiteEngineName(whiteName);
    gameRecord_.setBlackEngineName(blackName);

    for (auto& player : players_) {
        player->setStartPosition(gameRecord_);
    }

}

size_t GameContext::getPlayerCount() const {
    return players_.size();
}

PlayerContext* GameContext::player(size_t index) {
    if (index >= players_.size()) return nullptr;
    return players_[index].get();
}

PlayerContext* GameContext::getWhite() {
    if (players_.empty()) return nullptr;
    return players_[(switchedSide_ ? 1 : 0) % players_.size()].get();
}

PlayerContext* GameContext::getBlack() {
    if (players_.size() < 2) return getWhite();
    return players_[(switchedSide_ ? 0 : 1) % players_.size()].get();
}

void GameContext::setSideSwitched(bool switched) {
    switchedSide_ = switched;
}

bool GameContext::isSideSwitched() const {
    return switchedSide_;
}

void GameContext::setEventCallback(std::function<void(EngineEvent&&)> callback) {
    eventCallback_ = std::move(callback);
    for (auto& player : players_) {
        if (player->getEngine()) {
            player->getEngine()->setEventSink(eventCallback_);
        }
    }
}

GameRecord& GameContext::gameRecord() {
    return gameRecord_;
}

const GameRecord& GameContext::gameRecord() const {
    return gameRecord_;
}

std::tuple<GameEndCause, GameResult> GameContext::checkGameResult() {

    for (auto& player : players_) {
        auto [pcause, presult] = player->getGameResult();
        if (presult != GameResult::Unterminated) {
            gameRecord_.setGameEnd(pcause, presult);
            break;
        }
    }

    return gameRecord_.getGameResult();
}

bool GameContext::checkForTimeoutsAndRestart() {
    if (!eventCallback_) 
		throw AppError::make("GameContext::checkForTimeoutsAndRestart; No event callback set.");

    bool restarted = false;
    for (auto& player : players_) {
        if (player->checkEngineTimeout()) {
            restarted = true;
            player->getEngine()->setEventSink(eventCallback_);
        }
    }
    return restarted;
}

PlayerContext* GameContext::findPlayerByEngineId(const std::string& identifier) {
    for (auto& player : players_) {
        if (player->getEngine()->getIdentifier() == identifier) {
            return player.get();
        }
    }
    return nullptr;
}

void GameContext::restartIfConfigured() {
    for (auto& player : players_) {
        if (!player->getEngine()) continue;

        if (player->getEngine()->getConfig().getRestartOption() == RestartOption::Always) {
            player->restartEngine();
        }
    }
}

void GameContext::cancelCompute() {
    for (auto& player : players_) {
        player->cancelCompute();
    }
}

