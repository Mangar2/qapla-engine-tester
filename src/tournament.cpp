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

#include <sstream>
#include <iomanip>
#include <ctime>
#include "epd-reader.h"
#include "game-manager-pool.h"
#include "logger.h"
#include "tournament.h"
#include "pgn-io.h"
#include "engine-config-manager.h"

bool Tournament::wait() {
    GameManagerPool::getInstance().waitForTask();
    return true;
};

void Tournament::createTournament(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {
    if (!startPositions_) startPositions_ = std::make_shared<std::vector<std::string>>();

    if (config.openings.file.empty()) {
        Logger::testLogger().log("No openings file provided.", TraceLevel::error);
        return;
    }
    if (config.openings.format != "epd" && config.openings.format != "raw") {
        Logger::testLogger().log("Unsupported openings format: " + config.openings.format, TraceLevel::error);
        return;
    }
	engineConfig_ = engines;
    config_ = config;
    EpdReader reader(config.openings.file);
    for (const auto& entry : reader.all()) {
        if (!entry.fen.empty()) {
            startPositions_->push_back(entry.fen);
        }
    }

    if (startPositions_->empty()) {
        Logger::testLogger().log("No valid openings found in file.", TraceLevel::error);
        return;
    }

    PgnIO::tournament().initialize(config.event);
	if (config.type == "gauntlet") {
        createGauntletPairings(engines, config);
	}
    else if (config.type == "round-robin") {
        createRoundRobinPairings(engines, config);
    }
    else {
		Logger::testLogger().log("Unsupported tournament type: " + config.type, TraceLevel::error);
		return;
	}
}

void Tournament::createGauntletPairings(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {
    std::vector<EngineConfig> gauntlets;
    std::vector<EngineConfig> opponents;

    for (const auto& e : engines) {
        (e.isGauntlet() ? gauntlets : opponents).push_back(e);
    }

    if (gauntlets.empty() || opponents.empty()) {
        Logger::testLogger().log("Gauntlet tournament requires both gauntlet and opponent engines.", TraceLevel::error);
        return;
    }

    PairTournamentConfig ptc;
    ptc.games = config.games * config.rounds;
    ptc.repeat = config.repeat;
    ptc.swapColors = !config.noSwap;
    ptc.openings = config.openings;

    for (const auto& g : gauntlets) {
        for (const auto& o : opponents) {
            auto pt = std::make_shared<PairTournament>();
            pt->initialize(g, o, ptc, startPositions_);
            pairings_.push_back(std::move(pt));
        }
    }
}

void Tournament::createRoundRobinPairings(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {

    if (engines.size() < 2) {
        Logger::testLogger().log("Round-robin tournament requires at least two engines.", TraceLevel::error);
        return;
    }

    PairTournamentConfig ptc;
    ptc.games = config.games * config.rounds;
    ptc.repeat = config.repeat;
    ptc.swapColors = !config.noSwap;
    ptc.openings = config.openings;

    for (size_t i = 0; i < engines.size(); ++i) {
        for (size_t j = i + 1; j < engines.size(); ++j) {
            auto pt = std::make_shared<PairTournament>();
            pt->initialize(engines[i], engines[j], ptc, startPositions_);
            pairings_.push_back(std::move(pt));
        }
    }
}


void Tournament::scheduleAll(int concurrency) {
	GameManagerPool::getInstance().setConcurrency(concurrency, true);
	for (const auto& pairing : pairings_) {
		pairing->schedule();
	}
}

void Tournament::saveAll(std::ostream& out) const {
    for (const auto& config : engineConfig_) {
        out << config << "\n";
    }

    const int pairingsPerRound = static_cast<int>(pairings_.size()) / config_.rounds;

    for (size_t i = 0; i < pairings_.size(); ++i) {
        auto pairing = pairings_[i];
        const std::string line = pairing->toString();
        const auto sep = line.find(':');
        if (sep == std::string::npos) continue;

        const std::string engineNames = line.substr(0, sep - 1);
        const std::string result = line.substr(sep + 2);
        const int roundIndex = static_cast<int>(i) / pairingsPerRound + 1;

        out << "[round " << roundIndex << ": " << engineNames << "]\n";
        out << "games: " << result << "\n";

        const EngineDuelResult& resultData = pairing->getResult();
        const auto& stats = resultData.causeStats;

        bool hasAny = std::any_of(stats.begin(), stats.end(), [](const CauseStats& s) {
            return s.win > 0 || s.draw > 0 || s.loss > 0;
            });

        if (!hasAny) continue;

        std::string seperator;

        out << "winCauses: ";
        for (size_t i = 0; i < stats.size(); ++i) {
            if (stats[i].win > 0) {
                out << seperator << to_string(static_cast<GameEndCause>(i)) << ":" << stats[i].win;
                seperator = ",";
            }
        }
        out << "\n";

        seperator.clear();
        out << "drawCauses: ";
        for (size_t i = 0; i < stats.size(); ++i) {
            if (stats[i].draw > 0) {
                out << seperator << to_string(static_cast<GameEndCause>(i)) << ":" << stats[i].draw;
                seperator = ",";
            }
        }
        out << "\n";

        seperator.clear();
        out << "lossCauses: ";
        for (size_t i = 0; i < stats.size(); ++i) {
            if (stats[i].loss > 0) {
                out << seperator << to_string(static_cast<GameEndCause>(i)) << ":" << stats[i].loss;
                seperator = ",";
            }
        }
        out << "\n";
    }
}

std::unordered_set<std::string> Tournament::parseValidEngineNamesFromConfigs(std::istream& in) const {
    EngineConfigManager configLoader;
    configLoader.loadFromStream(in);
    const auto& loadedConfigs = configLoader.getAllConfigs();

    std::unordered_set<std::string> valid;

    for (const auto& loaded : loadedConfigs) {
        for (const auto& existing : engineConfig_) {
            if (loaded == existing) {
                valid.insert(loaded.getName());
                break;
            }
        }
    }

    return valid;
}

inline std::string trim(std::string_view sv) {
    const auto begin = sv.find_first_not_of(" \t\r\n");
    const auto end = sv.find_last_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return {};
    return std::string(sv.substr(begin, end - begin + 1));
}

std::pair<std::string, std::string> parseEngineNamesFromRoundLine(const std::string& line) {
    const auto colonPos = line.find(':');
    const auto vsPos = line.find(" vs ", colonPos);
    const auto closePos = line.find(']', vsPos);

    if (colonPos == std::string::npos || vsPos == std::string::npos || closePos == std::string::npos) {
        throw std::runtime_error("Invalid round header format: " + line);
    }

    const std::string engineA = trim(line.substr(colonPos + 1, vsPos - colonPos - 1));
    const std::string engineB = trim(line.substr(vsPos + 4, closePos - vsPos - 4));

    return { engineA, engineB };
}

PairTournament* Tournament::findMatchingPairing(const std::string& engineA, const std::string& engineB) const {
    for (const auto& pairing : pairings_) {
        const auto& p = pairing->getResult();
        const bool matchDirect = p.engineA == engineA && p.engineB == engineB;
        const bool matchReverse = p.engineA == engineB && p.engineB == engineA;
        if (matchDirect || matchReverse) return pairing.get();
    }
    return nullptr;
}

/**
 * @brief Parses a summary string like "W:3 D:1 L:2" and updates the result counts.
 * @param text The summary portion of the line after "games: ".
 * @param result The result object to update.
 */
void parseGameSummary(std::string_view text, EngineDuelResult& result) {
    std::istringstream iss(std::string{ text });
    std::string token;

    while (iss >> token) {
        if (token.starts_with("W:")) {
            result.winsEngineA = std::stoi(token.substr(2));
        }
        else if (token.starts_with("D:")) {
            result.draws = std::stoi(token.substr(2));
        }
        else if (token.starts_with("L:")) {
            result.winsEngineB = std::stoi(token.substr(2));
        }
    }
}

/**
 * @brief Parses a list of end causes in the format "cause1:count,cause2:count,..."
 *        and updates the specified field in each CauseStats entry.
 * @param text Comma-separated list of cause:count entries.
 * @param result EngineDuelResult to update.
 * @param field Pointer to the CauseStats member to increment (win/draw/loss).
 */
void parseEndCauses(std::string_view text, EngineDuelResult& result, int CauseStats::* field) {
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


std::string Tournament::parseRound(std::istream& in, const std::string& roundHeader,
    const std::unordered_set<std::string>& validEngines) {
	auto [engineA, engineB] = parseEngineNamesFromRoundLine(roundHeader);

    EngineDuelResult parsedResult;
    parsedResult.engineA = engineA;
    parsedResult.engineB = engineB;

    std::string line;
    while (std::getline(in, line)) {
        if (line.starts_with("[round ")) {
            break;
        }
        if (line.starts_with("games: ")) {
            parseGameSummary(line.substr(7), parsedResult);
        }
        else if (line.starts_with("winReasons: ")) {
            parseEndCauses(line.substr(12), parsedResult, &CauseStats::win);
        }
        else if (line.starts_with("drawReasons: ")) {
            parseEndCauses(line.substr(13), parsedResult, &CauseStats::draw);
        }
        else if (line.starts_with("lossReasons: ")) {
            parseEndCauses(line.substr(13), parsedResult, &CauseStats::loss);
        }
    }

    if (validEngines.contains(engineA) && validEngines.contains(engineB)) {
        if (auto* pairing = findMatchingPairing(engineA, engineB)) {
            pairing->getResult() += parsedResult;
        }
    }

    return line;
}



void Tournament::loadAll(std::istream& in) {
    std::stringstream configStream;
    std::string line;

    // Lese alle Konfig-Zeilen bis zur ersten Rundendefinition
    while (std::getline(in, line)) {
        if (line.starts_with("[round ")) break;
        configStream << line << "\n";
    }

    // Ermittle gültige Engines aus Konfig-Block
    const std::unordered_set<std::string> validEngines =
        parseValidEngineNamesFromConfigs(configStream);

    // Runde für Runde einlesen
    while (!line.empty()) {
        line = parseRound(in, line, validEngines);
    }
}



