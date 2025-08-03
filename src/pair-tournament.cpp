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

#include <random>
#include <iomanip>
#include "pair-tournament.h"
#include "game-manager-pool.h"
#include "pgn-io.h"

void PairTournament::initialize(const EngineConfig& engineA, const EngineConfig& engineB,
	const PairTournamentConfig& config, std::shared_ptr<StartPositions> startPositions) {

    std::lock_guard<std::mutex> lock(mutex_);
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

void PairTournament::schedule(const std::shared_ptr<PairTournament>& self) {

    if (!started_) {
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

    GameManagerPool::getInstance().addTaskProvider(self, engineA_, engineB_);
    GameManagerPool::getInstance().assignTaskToManagers();
}

uint32_t PairTournament::newOpeningIndex(size_t gameInEncounter) {
    if (config_.openings.order == "random") {
        std::uniform_int_distribution<size_t> dist(0, startPositions_->size() - 1);
        return static_cast<uint32_t>(dist(rng_));
    }
    else {
        uint32_t size = startPositions_->size();
        return (gameInEncounter / config_.repeat + config_.openings.start) % size;
    }
} 

void PairTournament::updateOpening(uint32_t openingIndex) {
    GameState gameState;
    openingIndex_ = openingIndex;
    if (startPositions_->fens.empty()) {
        curRecord_ = gameState.setFromGameRecord(startPositions_->games[openingIndex], config_.openings.plies);
    }
    else {
        auto& fen = startPositions_->fens[openingIndex];
        gameState.setFen(false, fen);
        curRecord_.setStartPosition(false, gameState.getFen(),
            gameState.isWhiteToMove(), engineA_.getName(), engineB_.getName());
    }
}

std::optional<GameTask> PairTournament::nextTask() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!started_ || !startPositions_ || startPositions_->empty()) {
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
            updateOpening(newOpeningIndex(i));
        }
        if (results_[i] != GameResult::Unterminated) {
            continue;
        }
        GameTask task;
        task.taskType = GameTask::Type::PlayGame;
		task.gameRecord = curRecord_;
		
		task.gameRecord.setRound(static_cast<uint32_t>(i + 1));
		task.taskId = std::to_string(i);
        task.switchSide = config_.swapColors && (i % 2 == 1);
        auto& white = task.switchSide ? engineB_ : engineA_;
        auto& black = task.switchSide ? engineA_ : engineB_;
        task.gameRecord.setTimeControl(white.getTimeControl(), black.getTimeControl());

        results_[i] = GameResult::Unterminated;
        nextIndex_ = i + 1;

        std::ostringstream oss;
        std::cout << std::left
            << "started round " << std::setw(3) << (config_.round + 1)
            << " game " << std::setw(3) << i + 1
            << " opening " << std::setw(6) << openingIndex_
            << " engines " << white.getName() << " vs " << black.getName()
            << std::endl;

        return task;
    }

    return std::nullopt;
}

void PairTournament::setGameRecord([[maybe_unused]] const std::string& taskId, const GameRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [cause, result] = record.getGameResult();
    uint32_t round = record.getRound();
    

    if (round == 0 || round > results_.size()) {
		Logger::testLogger().log("Invalid round number in GameRecord: Round " + std::to_string(round) 
            + " but having " + std::to_string(results_.size()) + " games started ", TraceLevel::error);
        return;
    }

    // Result is stored as "white-view", thus not engine-view. To count how often the engines won,
    // we need to check the color of the engine in this round.
    results_[round - 1] = result;
    GameRecord pgnRecord = record;
    pgnRecord.setRound(round + config_.gameNumberOffset);
    PgnIO::tournament().saveGame(pgnRecord);

	duelResult_.addResult(record);
    if (verbose_) {
        std::ostringstream oss;
        oss << std::left
            << "  match round " << std::setw(3) << (config_.round + 1)
            << " game " << std::setw(3) << round
            << " result " << std::setw(7) << to_string(result)
            << " cause " << std::setw(21) << to_string(cause)
            << " engines " << record.getWhiteEngineName() << " vs " << record.getBlackEngineName();
        Logger::testLogger().log(oss.str(), TraceLevel::result);
    }

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
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
	oss << engineA_.getName() << " vs " << engineB_.getName() << " : " << getResultSequenceEngineView();
    return oss.str();
}

void PairTournament::fromString(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);

    // The index of the next game to play is derived from the results_ vector, not from nextIndex_.
    // nextIndex_ is only used to avoid rechecking already completed games in nextTask().
    // Initializing it to 0 allows nextTask() to scan results_ for unfinished games and schedule them accordingly.
    nextIndex_ = 0; 

    auto pos = line.find(": ");
    if (pos == std::string::npos) return;

    std::string resultString = line.substr(pos + 2);
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

void PairTournament::trySaveIfNotEmpty(std::ostream& out) const {

    const auto& stats = duelResult_.causeStats;
    if (results_.empty()) return;

    out << "[round " << (config_.round + 1) << " engines " 
        << getEngineA().getName() << " vs " << getEngineB().getName() << "]\n";
    out << "games: " << getResultSequenceEngineView() << "\n";

    auto writeStats = [&](const char* label, auto accessor) {
        out << label << ": ";
        std::string sep;
        for (size_t i = 0; i < stats.size(); ++i) {
            int value = accessor(stats[i]);
            if (value > 0) {
                out << sep << to_string(static_cast<GameEndCause>(i)) << ":" << value;
                sep = ",";
            }
        }
        out << "\n";
        };

    writeStats("wincauses", [](const CauseStats& s) { return s.win; });
    writeStats("drawcauses", [](const CauseStats& s) { return s.draw; });
    writeStats("losscauses", [](const CauseStats& s) { return s.loss; });
}

std::tuple<uint32_t, std::string, std::string> PairTournament::parseRoundHeader(const std::string& line) {
    const std::string trimmed = trim(line);
    if (!trimmed.starts_with("[") || !trimmed.ends_with("]")) {
        throw std::runtime_error("Invalid round header: missing [ or ]\nLine: " + line);
    }

    const std::string inner = trimmed.substr(1, trimmed.size() - 2); // strip brackets

    const std::string prefix = "round ";
    const auto prefixPos = inner.find(prefix);
    if (prefixPos != 0) {
        throw std::runtime_error("Invalid round header: must start with 'round'\nLine: " + line);
    }

    const auto enginesPos = inner.find(" engines ", prefix.size());
    if (enginesPos == std::string::npos) {
        throw std::runtime_error("Invalid round header: missing 'engines'\nLine: " + line);
    }

    const std::string roundStr = trim(inner.substr(prefix.size(), enginesPos - prefix.size()));
    int round = std::stoi(roundStr);
    if (round < 0) {
        throw std::runtime_error("Invalid round number: " + roundStr + "\nLine: " + line);
	}

    const auto vsPos = inner.find(" vs ", enginesPos + 9);
    if (vsPos == std::string::npos) {
        throw std::runtime_error("Invalid round header: missing 'vs'\nLine: " + line);
    }

    const std::string engineA = trim(inner.substr(enginesPos + 9, vsPos - (enginesPos + 9)));
    const std::string engineB = trim(inner.substr(vsPos + 4));

    return { static_cast<uint32_t>(round), engineA, engineB };
}

bool PairTournament::matches(uint32_t round, const std::string& engineA, const std::string& engineB) const {
    return config_.round == round &&
        getEngineA().getName() == engineA &&
        getEngineB().getName() == engineB;
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
        if (sep == std::string::npos) continue;

        const std::string causeStr = trim(token.substr(0, sep));
        const int count = std::stoi(token.substr(sep + 1));

        const auto causeOpt = tryParseGameEndCause(causeStr);
        if (!causeOpt) continue;

        result.causeStats[static_cast<size_t>(*causeOpt)].*field += count;
    }
}

std::string PairTournament::load(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = trim(line);
		if (trimmed.empty()) continue; 
        if (trimmed.starts_with("[")) {
            const std::string afterBracket = trim(trimmed.substr(1));
            if (afterBracket.starts_with("round")) {
                // We need this line to parse the next entry
                return line; 
            }
        }

        if (line.starts_with("games: ")) {
            fromString(line);
        }
        else if (line.starts_with("wincauses: ")) {
            parseEndCauses(line.substr(11), duelResult_, &CauseStats::win);
        }
        else if (line.starts_with("drawcauses: ")) {
            parseEndCauses(line.substr(12), duelResult_, &CauseStats::draw);
        }
        else if (line.starts_with("losscauses: ")) {
            parseEndCauses(line.substr(12), duelResult_, &CauseStats::loss);
        }
    }
    return "";
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
