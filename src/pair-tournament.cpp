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

#include "game-result.h"
#include "pair-tournament.h"
#include "game-manager-pool.h"
#include "pgn-io.h"
#include "string-helper.h"

#include <random>
#include <iomanip>

namespace QaplaTester {

void PairTournament::initialize(const EngineConfig& engineA, const EngineConfig& engineB,
	const PairTournamentConfig& config, std::shared_ptr<StartPositions> startPositions) {

    std::scoped_lock lock(mutex_);
    if (initialized_) {
        throw std::logic_error("PairTournament already initialized");
    }
    if (!results_.empty()) {
        throw std::logic_error("PairTournament already has result data; call load() only after initialize()");
    }
    initialized_ = true;

    engineA_ = engineA;
    engineB_ = engineB;
    config_ = config;
    rng_.seed(config_.seed);
	startPositions_ = std::move(startPositions);

    if (config_.openings.policy == "encounter"|| config_.openings.policy == "round") {
        updateOpening(newOpeningIndex(0));
    }

	duelResult_ = EngineDuelResult(engineA_.getName(), engineB_.getName());
    bool white = true;
    for (const auto& r : results_) {

        switch (r) {
        case GameResult::WhiteWins:
            white ? ++duelResult_.winsEngineA : ++duelResult_.winsEngineB;
            break;
        case GameResult::BlackWins:
			white ? ++duelResult_.winsEngineB : ++duelResult_.winsEngineA;
            break;
        case GameResult::Draw:
			++duelResult_.draws;
            break;
        default:
            break;
        }
        if (config_.swapColors) {
			white = !white; 
        }
    }

}

void PairTournament::schedule(const std::shared_ptr<PairTournament>& self, GameManagerPool& pool) {

    if (!initialized_) {
        throw std::logic_error("PairTournament must be initialized before scheduling");
    }

    int remainingGames = std::max(0, static_cast<int>(config_.games - results_.size()));
    
	// We support having more games in results_ than config_.games.
	// Unfinished games in the first config_.games are played even, if there are more results.
    for (size_t i = 0; i < results_.size() && i < config_.games; i++) {
        if (results_[i] == GameResult::Unterminated) {
            ++remainingGames;
        }
    }

    if (remainingGames <= 0) {
        return; // Nichts zu tun
    }

    pool.addTaskProvider(self, engineA_, engineB_);
    pool.startManagers();
}

uint32_t PairTournament::newOpeningIndex(size_t gameInEncounter) {
    if (config_.openings.order == "random") {
        std::uniform_int_distribution<size_t> dist(0, startPositions_->size() - 1);
        return static_cast<uint32_t>(dist(rng_));
    }
    uint32_t size = startPositions_->size();
    return (gameInEncounter / config_.repeat + config_.openings.start) % size;
} 

void PairTournament::updateOpening(uint32_t openingIndex) {
    GameState gameState;
    openingIndex_ = openingIndex;
    if (startPositions_->fens.empty()) {
        curRecord_ = gameState.setFromGameRecordAndCopy(startPositions_->games[openingIndex], config_.openings.plies);
    }
    else {
        auto& fen = startPositions_->fens[openingIndex];
        gameState.setFen(false, fen);
        curRecord_.setStartPosition(false, gameState.getFen(), gameState.isWhiteToMove(), gameState.getStartHalfmoves(),
            engineA_.getName(), engineB_.getName());
    }
}

std::optional<GameTask> PairTournament::nextTask() {
    std::scoped_lock lock(mutex_);

    if (!initialized_ || !startPositions_ || startPositions_->empty()) {
        return std::nullopt;
    }

    if (nextIndex_ == 0){
        Logger::testLogger().log(getTournamentInfo(), TraceLevel::result);
    } 

    // Ensures robustness against unfinished games by scanning results_ instead of relying solely on nextIndex_.
    for (size_t i = nextIndex_; i < config_.games; ++i) {
        if (i >= results_.size()) {
            results_.resize(i + 1, GameResult::Unterminated);
        }
        // Ensures consistent opening assignment for replayed games,  
        // avoiding mismatches due to skipped entries in rotating schemes.
        if (config_.openings.policy == "default" && i % config_.repeat == 0) {
            auto openingIndex = newOpeningIndex(i); 
            updateOpening(openingIndex);
        }
        if (results_[i] != GameResult::Unterminated) {
            continue;
        }
        GameTask task;
        task.taskType = GameTask::Type::PlayGame;
		task.gameRecord = curRecord_;

		task.gameRecord.setTournamentInfo(
            static_cast<uint32_t>(config_.round + 1),
            static_cast<uint32_t>(i + 1),
            openingIndex_
        );
		task.taskId = std::to_string(i);
        task.switchSide = config_.swapColors && (i % 2 == 1);
        auto& white = task.switchSide ? engineB_ : engineA_;
        auto& black = task.switchSide ? engineA_ : engineB_;
        task.gameRecord.setTimeControl(white.getTimeControl(), black.getTimeControl());
        if (!positionName_.empty()) {
            task.gameRecord.setPositionName(positionName_ + " " + std::to_string(i + 1));
        }

        results_[i] = GameResult::Unterminated;
        nextIndex_ = i + 1;

        std::ostringstream oss;
        std::cout << std::left
            << "started round " << std::setw(3) << (config_.round + 1)
            << " game " << std::setw(3) << i + 1
            << " opening " << std::setw(6) << openingIndex_
            << " engines " << white.getName() << " vs " << black.getName()
            << "\n" << std::flush;

        return task;
    }

    return std::nullopt;
}

void PairTournament::setGameRecord([[maybe_unused]] const std::string& taskId, const GameRecord& record) {
    std::scoped_lock lock(mutex_);

    auto [cause, result] = record.getGameResult();
    uint32_t gameInRound = record.getGameInRound();
    

    if (gameInRound == 0 || gameInRound > results_.size()) {
		Logger::testLogger().log("Invalid round number in GameRecord: Round " + std::to_string(gameInRound) 
            + " but having " + std::to_string(results_.size()) + " games started ", TraceLevel::error);
        return;
    }

    // Result is stored as "white-view", thus not engine-view. To count how often the engines won,
    // we need to check the color of the engine in this round.
    results_[gameInRound - 1] = result;
    GameRecord pgnRecord = record;
    pgnRecord.setTotalGameNo(gameInRound + config_.gameNumberOffset);
    PgnIO::tournament().saveGame(pgnRecord);

	duelResult_.addResult(record);
    if (verbose_) {
        std::ostringstream oss;
        oss << std::left
            << "  match round " << std::setw(3) << (config_.round + 1)
            << " game " << std::setw(3) << gameInRound
            << " result " << std::setw(7) << to_string(result)
            << " cause " << std::setw(21) << to_string(cause)
            << " engines " << record.getWhiteEngineName() << " vs " << record.getBlackEngineName();
        Logger::testLogger().log(oss.str(), TraceLevel::result);
    }

    isFinished_ = std::cmp_greater_equal(duelResult_.total(), config_.games);

    if (onGameFinished_){
        onGameFinished_(this);
    } 

}

std::string PairTournament::getResultSequenceEngineView() const {
    std::ostringstream oss;
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
        default:
            oss << '?';
            break;
        }
    }
    return oss.str();
}

std::string PairTournament::toString() const {
    std::scoped_lock lock(mutex_);
    std::ostringstream oss;
	oss << engineA_.getName() << " vs " << engineB_.getName() << " : " << getResultSequenceEngineView();
    return oss.str();
}

void PairTournament::fromString(const std::string& line) {
    std::scoped_lock lock(mutex_);

    // The index of the next game to play is derived from the results_ vector, not from nextIndex_.
    // nextIndex_ is only used to avoid rechecking already completed games in nextTask().
    // Initializing it to 0 allows nextTask() to scan results_ for unfinished games and schedule them accordingly.
    nextIndex_ = 0; 
    std::string resultString = line;

    auto pos = line.find(": ");
    if (pos != std::string::npos) {
        resultString = line.substr(pos + 2);
    }

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
        case '=':
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

std::optional<QaplaHelpers::IniFile::Section> PairTournament::getSectionIfNotEmpty(const std::string& id) const {
    if (results_.empty()) {
        return std::nullopt;
    }

    QaplaHelpers::IniFile::Section section;
    section.name = "round";
    section.addEntry("id", id);
    section.addEntry("round", std::to_string(config_.round + 1));
    section.addEntry("engineA", getEngineA().getName());
    section.addEntry("engineB", getEngineB().getName());

    const auto& stats = duelResult_.causeStats;
    section.addEntry("games", getResultSequenceEngineView());

    auto addStats = [&](const std::string& label, auto accessor) {
        for (size_t i = 0; i < stats.size(); ++i) {
            int value = accessor(stats[i]);
            if (value > 0) {
                section.addEntry(label, to_string(static_cast<GameEndCause>(i)) + ":" + std::to_string(value));
            }
        }
        };

    addStats("wincauses", [](const CauseStats& s) { return s.win; });
    addStats("drawcauses", [](const CauseStats& s) { return s.draw; });
    addStats("losscauses", [](const CauseStats& s) { return s.loss; });
    
    return section;
}

bool PairTournament::matches(uint32_t round, const std::string& engineA, const std::string& engineB) const {
    return config_.round == round &&
        getEngineA().getName() == engineA &&
        getEngineB().getName() == engineB;
}

bool PairTournament::matches(const PairTournament& other) const {
    return matches(other.config_.round, other.getEngineA().getName(), other.getEngineB().getName());
}

/**
 * @brief Parses a list of end causes in the format "cause1:count,cause2:count,..."
 *        and updates the specified field in each CauseStats entry.
 * @param text Comma-separated list of cause:count entries.
 * @param result EngineDuelResult to update.
 * @param field Pointer to the CauseStats member to increment (win/draw/loss).
 */
static void parseEndCauses(std::string_view text, EngineDuelResult& result, int CauseStats::* field) {
    std::istringstream ss(std::string{ text });
    std::string token;

    while (std::getline(ss, token, ',')) {
        const auto sep = token.find(':');
        if (sep == std::string::npos) {
            continue;
        }

        const std::string causeStr = QaplaHelpers::trim(token.substr(0, sep));
        const int count = std::stoi(token.substr(sep + 1));

        const auto causeOpt = tryParseGameEndCause(causeStr);
        if (!causeOpt) {
            continue;
        }

        result.causeStats[static_cast<size_t>(*causeOpt)].*field += count;
    }
}

void PairTournament::fromSection(const QaplaHelpers::IniFile::Section& section) {
    std::string line;
    for (const auto& entry : section.entries) {
        auto [key, value] = entry;
        if (key == "games") {
            fromString(value);
        }
        else if (key.starts_with("wincauses")) {
            parseEndCauses(value, duelResult_, &CauseStats::win);
        }
        else if (key.starts_with("drawcauses")) {
            parseEndCauses(value, duelResult_, &CauseStats::draw);
        }
        else if (key.starts_with("losscauses")) {
            parseEndCauses(value, duelResult_, &CauseStats::loss);
        }
    }
    isFinished_ = std::cmp_greater_equal(duelResult_.total(), config_.games);
}

std::string PairTournament::getTournamentInfo() const {
    std::ostringstream oss;
    oss << "\nEncounter " << engineA_.getName() << " vs " << engineB_.getName()
        << " round " << (config_.round + 1)
        << " games " << config_.games
        << " repeat " << config_.repeat
        << " swap " << (config_.swapColors ? "yes" : "no")
        << "";
    return oss.str();
}

} // namespace QaplaTester
