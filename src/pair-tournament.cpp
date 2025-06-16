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

#include "pair-tournament.h"
#include "game-manager-pool.h"
#include "pgn-io.h"

void PairTournament::initialize(const EngineConfig& engineA, const EngineConfig& engineB,
	const PairTournamentConfig& config, std::shared_ptr<std::vector<std::string>> startPositions) {

    std::lock_guard lock(mutex_);
    if (started_) {
        throw std::logic_error("PairTournament already initialized");
    }
    if (!results_.empty()) {
        throw std::logic_error("PairTournament already has result data; call load() only after initialize()");
    }
    started_ = true;

    engineA_ = engineA;
    engineB_ = engineB;
    config_ = config;
	startPositions_ = std::move(startPositions);

	duelResult_.engineA = engineA_.getName();
	duelResult_.engineB = engineB_.getName();

    for (const auto& r : results_) {
        bool aWhite = true;
        size_t i = &r - results_.data();
        if (config_.swapColors) {
            aWhite = (i % 2 == 0);
        }

        switch (r) {
        case GameResult::WhiteWins:
            aWhite ? ++duelResult_.winsEngineA : ++duelResult_.winsEngineB;
            break;
        case GameResult::BlackWins:
			aWhite ? ++duelResult_.winsEngineB : ++duelResult_.winsEngineA;
            break;
        case GameResult::Draw:
			++duelResult_.draws;
            break;
        default:
            break;
        }
    }

}

void PairTournament::schedule() {

    if (!started_) {
        throw std::logic_error("PairTournament must be initialized before scheduling");
    }

    // Nur verbleibende Spiele berücksichtigen
    int remainingGames = static_cast<int>(config_.games - results_.size());
    for (const auto& result : results_) {
        if (result == GameResult::Unterminated) {
            ++remainingGames;
        }
    }

    if (remainingGames == 0) {
        return; // Nichts zu tun
    }

    GameManagerPool::getInstance().addTaskProvider(
        this,
        engineA_,
        engineB_,
        remainingGames
    );
}

std::optional<GameTask> PairTournament::nextTask(
    [[maybe_unused]] const std::string& whiteId,
    [[maybe_unused]] const std::string& blackId) {
    std::lock_guard lock(mutex_);

    if (!started_ || !startPositions_ || startPositions_->empty()) {
        return std::nullopt;
    }

    for (size_t i = nextIndex_; i < config_.games; ++i) {
        if (i >= results_.size()) {
            results_.resize(i + 1, GameResult::Unterminated);
        }

        if (results_[i] != GameResult::Unterminated) {
            continue;
        }

        int openingIndex;
        if (i % config_.repeat == 0) {
            if (config_.openings.order == "random") {
                openingIndex = static_cast<size_t>(std::rand()) % startPositions_->size();
            }
            else {
				int size = static_cast<int>(startPositions_->size()) - config_.openings.start;
                if (size <= 0) {
					Logger::testLogger().log("Invalid start position configuration: openings.start is too high.", 
                        TraceLevel::warning);
                    config_.openings.start = 0;
					size = static_cast<int>(startPositions_->size());
                }
                openingIndex = ((i / config_.repeat) % size) + config_.openings.start;
            }
            curStartPosition_ = (*startPositions_)[openingIndex];
            GameState gameState;
            gameState.setFen(false, curStartPosition_);
            curStartPosition_ = gameState.getFen();
        }

        GameTask task;
        task.taskType = GameTask::Type::PlayGame;
        task.useStartPosition = false;
        task.fen = curStartPosition_;
        task.whiteTimeControl = engineA_.getTimeControl();
        task.blackTimeControl = engineB_.getTimeControl();
        task.switchSide = config_.swapColors && (i % 2 == 1);
        task.round = static_cast<uint32_t>(i + 1);

        results_[i] = GameResult::Unterminated;
        nextIndex_ = i + 1;

        return task;
    }

    return std::nullopt;
}

void PairTournament::setGameRecord(const std::string& whiteId,
    const std::string& blackId,
    const GameRecord& record) {
    std::lock_guard lock(mutex_);

    auto [cause, result] = record.getGameResult();
    uint32_t round = record.getRound();
    

    if (round == 0 || round > results_.size()) {
		Logger::testLogger().log("Invalid round number in GameRecord: Round " + std::to_string(round) 
            + " but having " + std::to_string(results_.size()) + " games started ", TraceLevel::error);
        return;
    }

    // Speichern
    results_[round - 1] = result;
    PgnIO::tournament().saveGame(record);
	duelResult_.addResult(record);
    Logger::testLogger().log(duelResult_.toString() + " " + to_string(cause), TraceLevel::result);
}

std::string PairTournament::toString() const {
    std::lock_guard lock(mutex_);
    std::ostringstream oss;
    oss << engineA_.getName() << " vs " << engineB_.getName() << " : ";
    for (size_t i = 0; i < results_.size(); ++i) {
        bool aWhite = !config_.swapColors || (i % 2 == 0);
        switch (results_[i]) {
        case GameResult::WhiteWins:
            oss << (aWhite ? '1' : '0');
            break;
        case GameResult::BlackWins:
            oss << (aWhite ? '0' : '1');
            break;
        case GameResult::Draw:
            oss << '=';
            break;
        case GameResult::Unterminated:
            oss << '?';
            break;
        }
    }
    return oss.str();
}


void PairTournament::fromString(const std::string& line) {
    std::lock_guard lock(mutex_);

    if (!started_) {
        throw std::logic_error("PairTournament must be initialized before loading");
    }

    auto pos = line.find(" : ");
    if (pos == std::string::npos) return;

    std::string resultString = line.substr(pos + 3);
	duelResult_.clear();
    results_.clear();
    results_.reserve(resultString.size());

    for (size_t i = 0; i < resultString.size(); ++i) {
        char ch = resultString[i];
        bool aWhite = !config_.swapColors || (i % 2 == 0);

        switch (ch) {
        case '1':
            results_.emplace_back(aWhite ? GameResult::WhiteWins : GameResult::BlackWins);
            ++duelResult_.winsEngineA;
            break;
        case '0':
            results_.emplace_back(aWhite ? GameResult::BlackWins : GameResult::WhiteWins);
            ++duelResult_.winsEngineB;
            break;
        case '½':
            results_.emplace_back(GameResult::Draw);
            ++duelResult_.draws;
            break;
        case '?':
        default:
            results_.emplace_back(GameResult::Unterminated);
            break;
        }
    }

}


