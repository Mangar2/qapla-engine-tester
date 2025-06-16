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

    winsEngineA_ = 0;
    winsEngineB_ = 0;
    draws_ = 0;

    for (const auto& r : results_) {
        bool aWhite = true;
        size_t i = &r - results_.data();
        if (config_.swapColors) {
            aWhite = (i % 2 == 0);
        }

        switch (r) {
        case GameResult::WhiteWins:
            aWhite ? ++winsEngineA_ : ++winsEngineB_;
            break;
        case GameResult::BlackWins:
            aWhite ? ++winsEngineB_ : ++winsEngineA_;
            break;
        case GameResult::Draw:
            ++draws_;
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
        task.whiteTimeControl = config_.timeControl;
        task.blackTimeControl = config_.timeControl;
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
    bool whiteIsEngine0 = engineA_.getName() == record.getWhiteEngineName();
    std::string resultText;

    switch (result) {
    case GameResult::WhiteWins:
        if (whiteIsEngine0) {
            ++winsEngineA_;
            resultText = engineA_.getName() + " (White) wins";
        }
        else {
            ++winsEngineB_;
            resultText = engineB_.getName() + " (White) wins";
        }
        break;
    case GameResult::BlackWins:
        if (whiteIsEngine0) {
            ++winsEngineB_;
            resultText = engineB_.getName() + " (Black) wins";
        }
        else {
            ++winsEngineA_;
            resultText = engineA_.getName() + " (Black) wins";
        }
        break;
    case GameResult::Draw:
        ++draws_;
        resultText = "Draw";
        break;
    case GameResult::Unterminated:
        resultText = "Unterminated game";
        break;
    }
    std::ostringstream oss;
	oss << engineA_.getName() << " vs " << engineB_.getName()
        << " W:" << winsEngineA_ << " D:" << draws_ << " L:" << winsEngineB_
        << " " << to_string(cause);
    Logger::testLogger().log(oss.str(), TraceLevel::result);
}

std::string PairTournament::toString() const {
    std::lock_guard lock(mutex_);
	std::ostringstream oss;
    oss << engineA_.getName() << " vs " << engineB_.getName() << " : ";
    for (const auto& result : results_) {
        switch (result) {
        case GameResult::WhiteWins: oss << '1'; break;
        case GameResult::BlackWins: oss << '0'; break;
        case GameResult::Draw:      oss << '½'; break;
        case GameResult::Unterminated: oss << '?'; break;
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
    results_.clear();
    results_.reserve(resultString.size());

    for (char ch : resultString) {
        switch (ch) {
        case '1': results_.emplace_back(GameResult::WhiteWins); break;
        case '0': results_.emplace_back(GameResult::BlackWins); break;
        case '½': results_.emplace_back(GameResult::Draw);      break;
        case '?': results_.emplace_back(GameResult::Unterminated); break;
        default:  results_.emplace_back(GameResult::Unterminated); break;
        }
    }

    winsEngineA_ = 0;
    winsEngineB_ = 0;
    draws_ = 0;

    for (size_t i = 0; i < results_.size(); ++i) {
        const auto& r = results_[i];
        bool aWhite = !config_.swapColors || (i % 2 == 0);

        switch (r) {
        case GameResult::WhiteWins:
            aWhite ? ++winsEngineA_ : ++winsEngineB_;
            break;
        case GameResult::BlackWins:
            aWhite ? ++winsEngineB_ : ++winsEngineA_;
            break;
        case GameResult::Draw:
            ++draws_;
            break;
        default:
            break;
        }
    }
}


