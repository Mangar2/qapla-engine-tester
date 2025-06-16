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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#include <sstream>
#include <iomanip>
#include <ctime>
#include "epd-reader.h"
#include "game-manager-pool.h"
#include "logger.h"
#include "tournament.h"
#include "pgn-io.h"

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
    out << "[meta]\n";
    out << "type=" << config_.type << "\n";
    out << "rounds=" << config_.rounds << "\n";
    out << "gamesPerRound=" << config_.games << "\n\n";

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
        bool hasCauses = std::any_of(resultData.causeCounters.begin(), resultData.causeCounters.end(),
            [](int count) { return count > 0; });

        if (hasCauses) {
            out << "endReasons: ";
            std::string separator;
            for (size_t i = 0; i < resultData.causeCounters.size(); ++i) {
                int count = resultData.causeCounters[i];
                if (count == 0) continue;
                out << separator << to_string(static_cast<GameEndCause>(i)) << ":" << count;
				separator = ",";
            }
            out << "\n\n";
        }
    }
}


