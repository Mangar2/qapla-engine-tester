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
#include <random>
#include "epd-reader.h"
#include "game-manager-pool.h"
#include "logger.h"
#include "tournament.h"
#include "pgn-io.h"
#include "engine-config-manager.h"
#include "input-handler.h"

bool Tournament::wait() {
    GameManagerPool::getInstance().waitForTask();
    return true;
};

void Tournament::createTournament(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {

    if (!startPositions_) startPositions_ = std::make_shared<StartPositions>();

    if (config.openings.file.empty()) {
        Logger::testLogger().log("No openings file provided.", TraceLevel::error);
        return;
    }

    engineConfig_ = engines;
    config_ = config;

    if (config.openings.format == "epd" || config.openings.format == "raw") {
        EpdReader reader(config.openings.file);
        for (const auto& entry : reader.all()) {
            if (!entry.fen.empty()) {
                startPositions_->fens.push_back(entry.fen);
            }
        }
    }
    else if (config.openings.format == "pgn") {
        PgnIO pgnReader;
        startPositions_->games = std::move(pgnReader.loadGames(config.openings.file));
    }
    else {
		throw AppError::makeInvalidParameters(
			"Unsupported openings format: " + config.openings.format);
        return;
    }

    if (startPositions_->fens.empty() && startPositions_->games.empty()) {
		throw AppError::makeInvalidParameters(
			"No valid openings found in file: " + config.openings.file);
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
        std::string message = "Gauntlet tournament requires both gauntlet and opponent engines. ";
        message += "Found: " + std::to_string(gauntlets.size()) + " gauntlet(s), ";
        message += std::to_string(opponents.size()) + " opponent(s).";
        throw AppError::makeInvalidParameters(message);
    }

    createPairings(gauntlets, opponents, config, false);
}

void Tournament::createRoundRobinPairings(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {

    if (engines.size() < 2) {
        Logger::testLogger().log("Round-robin tournament requires at least two engines.", TraceLevel::error);
        return;
    }

	createPairings(engines, engines, config, true);
}

void Tournament::createPairings(const std::vector<EngineConfig>& players, const std::vector<EngineConfig>& opponents,
    const TournamentConfig& config, bool symmetric) {
    int openingOffset = config.openings.start;
    std::mt19937 rng(config.openings.seed);    
    std::uniform_int_distribution<size_t> dist(0, startPositions_->size() - 1);
    int posSize = static_cast<int>(startPositions_->size());

    PairTournamentConfig ptc;
    ptc.games = config.games;
    ptc.repeat = config.repeat;
    ptc.swapColors = !config.noSwap;
    ptc.openings = config.openings;
    ptc.gameNumberOffset = 0;

    for (int round = 0; round < config.rounds; ++round) {
        ptc.round = round;
        ptc.seed = static_cast<int>(dist(rng));
        openingOffset %= posSize;
        ptc.openings.start = openingOffset;

        // In default, all pairings in a round use the same opening offset and the same seed.
        if (config.openings.policy == "default") { 
            openingOffset += (ptc.games + ptc.repeat - 1) / ptc.repeat;
        } else if (config.openings.policy == "round") {
            openingOffset++;
        }

        for (size_t i = 0; i < players.size(); ++i) {
            for (size_t j = symmetric ? i + 1 : 0; j < opponents.size(); ++j) {
                auto pt = std::make_shared<PairTournament>();
                if (config.openings.policy == "encounter") {
                    ptc.openings.start = openingOffset;
                    openingOffset = (openingOffset + 1) % posSize;
                    ptc.seed = static_cast<int>(dist(rng));
                }
                pt->initialize(players[i], opponents[j], ptc, startPositions_);
                pt->setGameFinishedCallback([this](PairTournament* sender) {
                    this->onGameFinished(sender);
                });
                pairings_.push_back(std::move(pt));
                ptc.gameNumberOffset += ptc.games;
            }
        }
    }

}

void Tournament::onGameFinished([[maybe_unused]] PairTournament*) {
    ++raitingTrigger_;
    ++outcomeTrigger_;

    if (config_.ratingInterval > 0 && raitingTrigger_ >= config_.ratingInterval) {
        raitingTrigger_ = 0;
        auto result = getResult();
        result.printRatingTableUciStyle(std::cout);
    }
    if (config_.outcomeInterval > 0 && outcomeTrigger_ >= config_.outcomeInterval) {
        outcomeTrigger_ = 0;
        auto result = getResult();
        result.printOutcome(std::cout);
    }
}

void Tournament::scheduleAll(int concurrency) {
	GameManagerPool::getInstance().setConcurrency(concurrency, true);
    tournamentCallback_ = InputHandler::getInstance().registerCommandCallback(
        InputHandler::ImmediateCommand::Info,
        [this](InputHandler::ImmediateCommand, InputHandler::CommandValue) {
			auto result = getResult();
            result.printRatingTableUciStyle(std::cout);
        });
	for (const auto& pairing : pairings_) {
		pairing->schedule();
	}
}

void Tournament::save(std::ostream& out) const {
    for (const auto& config : engineConfig_) {
        out << config << "\n";
    }

    for (auto & pairing : pairings_) {
        pairing->trySaveIfNotEmpty(out);
    }
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

std::string Tournament::loadRound(std::istream& in, const std::string& roundHeader,
    const std::unordered_set<std::string>& validEngines) {
    auto [round, engineA, engineB] = PairTournament::parseRoundHeader(roundHeader);

    if (validEngines.contains(engineA) && validEngines.contains(engineB)) {
        for (const auto& pairing : pairings_) {
            if (pairing->matches(round - 1, engineA, engineB)) {
                return pairing->load(in);
            }
        }
    }

    PairTournament tmp;
    return tmp.load(in);
}

void Tournament::load(std::istream& in) {
    std::stringstream configStream;
    std::string line;

    while (std::getline(in, line)) {
        if (line.starts_with("[round ")) break;
        configStream << line << "\n";
    }

    EngineConfigManager configLoader;
    configLoader.loadFromStream(configStream);
    const std::unordered_set<std::string> validEngines =
        configLoader.findMatchingNames(engineConfig_);

    while (!line.empty()) {
        // Every loader ensures that we have a round header as next line
        line = loadRound(in, line, validEngines);
    }
}
