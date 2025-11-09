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
#include <fstream>
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
#include "adjudication-manager.h"

namespace QaplaTester {

void Tournament::createTournament(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {

    if (!startPositions_) {
        startPositions_ = std::make_shared<StartPositions>();
    }

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
        startPositions_->games = pgnReader.loadGames(config.openings.file);
    }
    else {
		throw AppError::makeInvalidParameters(
			"Unsupported openings format: " + config.openings.format);
    }

    if (startPositions_->fens.empty() && startPositions_->games.empty()) {
		throw AppError::makeInvalidParameters(
			"No valid openings found in file: " + config.openings.file);
    }

    AppError::throwOnInvalidOption({ "gauntlet", "round-robin" }, config.type, "Unsupported tournament type");
    auto savedPairings = std::move(pairings_);
    pairings_.clear();
    if (config.type == "gauntlet") {
        createGauntletPairings(engines, config);
    }
    else {
        createRoundRobinPairings(engines, config);
    }
    restoreResults(savedPairings);
    updateCnt_ ++;
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
        throw AppError::make(message);
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
    uint32_t openingOffset = config.openings.start;
    std::mt19937 rng(config.openings.seed);    
    std::uniform_int_distribution<size_t> dist(0, startPositions_->size() - 1);
    auto posSize = startPositions_->size();

    PairTournamentConfig ptc;
    ptc.games = config.games;
    ptc.repeat = config.repeat;
    ptc.swapColors = !config.noSwap;
    ptc.openings = config.openings;
    ptc.gameNumberOffset = 0;

    for (uint32_t round = 0; round < config.rounds; ++round) {
        ptc.round = round;
        ptc.seed = static_cast<uint32_t>(dist(rng));
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
                auto pairTour = std::make_shared<PairTournament>();
                if (config.openings.policy == "encounter") {
                    ptc.openings.start = openingOffset;
                    openingOffset = (openingOffset + 1) % posSize;
                    ptc.seed = static_cast<uint32_t>(dist(rng));
                }
                pairTour->initialize(players[i], opponents[j], ptc, startPositions_);
                pairTour->setGameFinishedCallback([this](PairTournament* sender) {
                    this->onGameFinished(sender);
                });
                pairings_.push_back(std::move(pairTour));
                ptc.gameNumberOffset += ptc.games;
            }
        }
    }

}

void Tournament::onGameFinished([[maybe_unused]] PairTournament* sender) {
    ++raitingTrigger_;
    ++outcomeTrigger_;
    ++saveTrigger_;
    ++updateCnt_;
    {
        std::scoped_lock lock(stateMutex_);
        result_ = getResult();
    }
    if (config_.ratingInterval > 0 && raitingTrigger_ >= config_.ratingInterval) {
        raitingTrigger_ = 0;
        auto result = getResult();
        result.printRatingTableUciStyle(std::cout, config_.averageElo);
    }
    if (config_.outcomeInterval > 0 && outcomeTrigger_ >= config_.outcomeInterval) {
        outcomeTrigger_ = 0;
        auto result = getResult();
        result.printOutcome(std::cout);
    }
    if (config_.saveInterval > 0 && saveTrigger_ >= config_.saveInterval) {
        saveTrigger_ = 0;
        try {
            if (!config_.tournamentFilename.empty()) {
                save(config_.tournamentFilename);
            }
        } catch (const std::exception& ex) {
            Logger::testLogger().log("Error saving tournament state: " + std::string(ex.what()), TraceLevel::error);
        }
    }
}

void Tournament::scheduleAll(uint32_t concurrency, bool registerToInputhandler,  GameManagerPool& pool) {
	// Initialize PGN output - at this point all tournament data is loaded
	// Check if we're resuming by seeing if any pairing has existing results
	bool isResumingTournament = false;
	for (const auto& pairing : pairings_) {
		if (pairing->hasResults()) {
			isResumingTournament = true;
			break;
		}
	}
	PgnIO::tournament().initialize(config_.event, isResumingTournament);
	
	pool.setConcurrency(concurrency, true);
    if (registerToInputhandler) {
        tournamentCallback_ = InputHandler::getInstance().registerCommandCallback(
            {
                InputHandler::ImmediateCommand::Info,
                InputHandler::ImmediateCommand::Outcome
            },
            [this](InputHandler::ImmediateCommand cmd, [[maybe_unused]] const InputHandler::CommandValue& value) {
                auto result = getResult();
                if (cmd == InputHandler::ImmediateCommand::Info) {
                    result.printRatingTableUciStyle(std::cout, config_.averageElo);
                    QaplaTester::AdjudicationManager::poolInstance().printTestResult(std::cout);
                }
                else if (cmd == InputHandler::ImmediateCommand::Outcome) {
                    result.printOutcome(std::cout);
                }
            });
    }
	for (const auto& pairing : pairings_) {
		pairing->schedule(pairing, pool);
	}
}

std::vector<QaplaHelpers::IniFile::Section> Tournament::getSections() const {
    std::vector<QaplaHelpers::IniFile::Section> sections;
    
    for (const auto& pairing : pairings_) {
        auto section = pairing->getSectionIfNotEmpty("tournament");
        if (section.has_value()) {
            sections.push_back(std::move(section.value()));
        }
    }
    
    return sections;
}

void Tournament::save(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("Failed to open file for saving tournament results: " + filename);
    }
    
    // Write engine configurations
    for (const auto& config : engineConfig_) {
        out << config << "\n";
    }
    
    // Write tournament round sections
    auto sections = getSections();
    for (const auto& section : sections) {
        QaplaHelpers::IniFile::saveSection(out, section);
    }
}

void Tournament::restoreResults(const std::vector<std::shared_ptr<PairTournament>>& savedPairings) {
    for (const auto& saved : savedPairings) {
        for (const auto& pairing : pairings_) {
            if (pairing->matches(*saved)) 
            {
                pairing->copyResultsFrom(*saved);
                break;
            }
        }
    }
}

void Tournament::load(const QaplaHelpers::IniFile::Section& section) {
    std::string engineA;
    std::string engineB;
    uint32_t round = 0;
    std::string games;
    updateCnt_++;
    try {
        for (const auto& [key, value]: section.entries) {
            if (key == "engineA") {
                engineA = value;
            } else if (key == "engineB") {
                engineB = value;
            } else if (key == "round") {
                round = std::stoul(value) - 1;
            } else if (key == "games") {
                games = value;
            }
        }
        for (const auto& pairing : pairings_) {
            if (!games.empty() && pairing->matches(round, engineA, engineB)) {
                pairing->fromSection(section);
                break;
            }
        }
    } catch (const std::exception& ex) {
        // Ignore invalid section
    }
}

void Tournament::load(const QaplaHelpers::IniFile::SectionList& sections) {

    for (const auto& section : sections) {
        load(section);
    }
}

uint32_t Tournament::calculateTotalGames(const std::vector<EngineConfig>& engines, const TournamentConfig& config) {
    if (engines.empty()) {
        return 0;
    }

    // Separate gauntlets from opponents
    std::vector<EngineConfig> gauntlets;
    std::vector<EngineConfig> opponents;
    
    for (const auto& e : engines) {
        (e.isGauntlet() ? gauntlets : opponents).push_back(e);
    }

    // Determine tournament type and player/opponent counts
    size_t playerCount = 0;
    size_t opponentCount = 0;
    bool isSymmetric = false;

    if (config.type == "gauntlet") {
        if (gauntlets.empty() || opponents.empty()) {
            return 0; // Invalid gauntlet configuration
        }
        playerCount = gauntlets.size();
        opponentCount = opponents.size();
        isSymmetric = false;
    } else { // round-robin
        if (engines.size() < 2) {
            return 0; // Invalid round-robin configuration
        }
        playerCount = engines.size();
        opponentCount = engines.size();
        isSymmetric = true;
    }

    // Calculate number of pairings
    uint32_t pairingsPerRound = 0;
    if (isSymmetric) {
        // Round-robin: each player plays against each other player once per round
        // This is playerCount * (playerCount - 1) / 2
        pairingsPerRound = static_cast<uint32_t>(playerCount * (opponentCount - 1) / 2);
    } else {
        // Gauntlet: each gauntlet plays against each opponent
        pairingsPerRound = static_cast<uint32_t>(playerCount * opponentCount);
    }

    // Total games = pairings * rounds * games per pairing
    return pairingsPerRound * config.rounds * config.games;
}

} // namespace QaplaTester
