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

#include "tournament.h"
#include "../opening/opening-parser.h"
#include "../opening/pgn-save.h"

#include "../cli/input-handler.h"

#include "../game-manager/game-manager-pool.h"
#include "../game-manager/adjudication-manager.h"

#include "../base-elements/logger.h"
#include "../base-elements/string-helper.h"
#include "../engine-handling/run-header-logger.h"

#include <ctime>
#include <random>
#include <format>

namespace QaplaTester {

TableData Tournament::getRatingStatusTable() const {
    auto result = getResult();
    TableData table;
    table.columnWidths = { 4, 28, 8, 6, 5, 8, 8 };
    table.headers = { "Rank", "Name", "Elo", "Error", "Games", "Score", "Draw" };

    auto scoredEngines = result.computeAllElos(config_.averageElo);
    int rankValue = 1;
    for (const auto& scored : scoredEngines) {
        const auto aggregate = scored.result.aggregate(scored.engineName);
        const auto totalGames = aggregate.total();
        const auto scorePercent = scored.score * 100.0;
        const auto drawPercent = totalGames > 0 ? 100.0 * static_cast<double>(aggregate.draws) / static_cast<double>(totalGames) : 0.0;
        const auto hasReliableError = totalGames >= 10 && scored.error != 0;

        table.body.push_back({
            rankValue,
            scored.engineName,
            hasReliableError ? TableCell(scored.elo) : TableCell("n/a"),
            hasReliableError ? TableCell(scored.error) : TableCell("n/a"),
            totalGames,
            std::format("{:.2f}%", scorePercent),
            std::format("{:.1f}%", drawPercent)
        });
        ++rankValue;
    }

    return table;
}

TableData Tournament::getOutcomeStatusTable() const {
    const auto result = getResult();
    TableData table;
    table.columnWidths = { 28, 6, 6, 6, 6 };
    table.headers = { "Name", "Wins", "Loss", "Draw", "Total" };

    for (const auto& engineName : result.engineNames()) {
        const auto engineResult = result.forEngine(engineName);
        if (!engineResult.has_value()) {
            continue;
        }
        const auto aggregate = engineResult->aggregate(engineName);
        table.body.push_back({
            engineName,
            aggregate.winsEngineA,
            aggregate.winsEngineB,
            aggregate.draws,
            aggregate.total()
        });
    }

    return table;
}

void Tournament::createTournament(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {

    if (!startPositions_) {
        startPositions_ = std::make_shared<StartPositions>();
    }

    if (config.openings.file.empty()) {
        throw AppError::makeInvalidParameters("No openings file provided.");
    }

    engineConfig_ = engines;
    config_ = config;

    OpeningParser parser;
    startPositions_->games = parser.parse(config.openings.file);

    if (startPositions_->games.empty()) {
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
    changeTracker_.trackModification();

    RunHeaderLogger::log("Tournament", engines, {
        { "Event", config.event.empty() ? "-" : config.event },
        { "Type", config.type },
        { "Games per pairing", std::format("{}", config.games) },
        { "Rounds", std::format("{}", config.rounds) },
        { "Repeat", std::format("{}", config.repeat) },
        { "Rating interval", config.ratingInterval > 0 ? std::format("{} games", config.ratingInterval) : "off" },
        { "Outcome interval", config.outcomeInterval > 0 ? std::format("{} games", config.outcomeInterval) : "off" },
        { "Average Elo", std::format("{}", config.averageElo) },
        { "Color swap", config.noSwap ? "off" : "on" },
    });
}

void Tournament::partitionGauntletEngines(const std::vector<EngineConfig>& engines,
    std::vector<EngineConfig>& gauntlets, std::vector<EngineConfig>& opponents) {
    gauntlets.clear();
    opponents.clear();

    for (const auto& e : engines) {
        (e.isGauntlet() ? gauntlets : opponents).push_back(e);
    }

    if (gauntlets.empty() && !opponents.empty()) {
        gauntlets.push_back(opponents.front());
        opponents.erase(opponents.begin());
    }
}

void Tournament::createGauntletPairings(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {
    std::vector<EngineConfig> gauntlets;
    std::vector<EngineConfig> opponents;
    partitionGauntletEngines(engines, gauntlets, opponents);

    if (gauntlets.empty() || opponents.empty()) {
        throw AppError::make(
            std::format("Gauntlet tournament requires both gauntlet and opponent engines. Found: {} gauntlet(s), {} opponent(s).",
            gauntlets.size(), opponents.size())
        );
    }

    createPairings(gauntlets, opponents, config, false);
}

void Tournament::createRoundRobinPairings(const std::vector<EngineConfig>& engines,
    const TournamentConfig& config) {

    if (engines.size() < 2) {
        throw AppError::makeInvalidParameters("Round-robin tournament requires at least two engines.");
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
    {
        std::scoped_lock lock(stateMutex_);
        result_ = getResult();
        changeTracker_.trackUpdate();
    }
    if (config_.ratingInterval > 0 && raitingTrigger_ >= config_.ratingInterval) {
        raitingTrigger_ = 0;
        const auto table = getRatingStatusTable();
        Logger::reportLogger().logTable("ratingTable", table, TraceLevel::result);
    }
    if (config_.outcomeInterval > 0 && outcomeTrigger_ >= config_.outcomeInterval) {
        outcomeTrigger_ = 0;
        const auto table = getOutcomeStatusTable();
        Logger::reportLogger().logTable("outcome", table, TraceLevel::result);
    }
    
    checkAutoSave();
}

void Tournament::checkAutoSave() {
    bool allFinished = true;
    for (const auto& pairing : pairings_) {
        if (!pairing->isFinished()) {
            allFinished = false;
            break;
        }
    }

    saveTimer_.update(allFinished);
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
	PgnSave::tournament().initialize(config_.event, isResumingTournament);
	
    saveTimer_.update(true);

	pool.setConcurrency(concurrency, true);
    if (registerToInputhandler) {
        tournamentCallback_ = InputHandler::getInstance().registerCommandCallback(
            {
                InputHandler::ImmediateCommand::Info,
                InputHandler::ImmediateCommand::Outcome
            },
            [this, &pool](InputHandler::ImmediateCommand cmd, [[maybe_unused]] const InputHandler::CommandValue& value) {
                auto result = getResult();
                if (cmd == InputHandler::ImmediateCommand::Info) {
                    result.printRatingTableUciStyle(std::cout, config_.averageElo);
                    pool.getAdjudicationManager().printTestResult(std::cout);
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
    changeTracker_.trackModification();

    for (const auto& [key, value] : section.entries) {
        if (key == "engineA") {
            engineA = value;
        } else if (key == "engineB") {
            engineB = value;
        } else if (key == "round") {
            const auto val = QaplaHelpers::to_uint32(value);
            if (!val) {
                return;
            }
            round = *val - 1;
        } else if (key == "games") {
            games = value;
        }
    }

    if (games.empty()) {
        return;
    }

    for (const auto& pairing : pairings_) {
        if (pairing->matches(round, engineA, engineB)) {
            pairing->fromSection(section);
            break;
        }
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

    // Determine tournament type and player/opponent counts
    size_t playerCount = 0;
    size_t opponentCount = 0;
    bool isSymmetric = false;

    if (config.type == "gauntlet") {
        partitionGauntletEngines(engines, gauntlets, opponents);
        if (gauntlets.empty() || opponents.empty()) {
            return 0; // Invalid gauntlet configuration (fewer than 2 engines)
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

void Tournament::save() {
    saveTimer_.update(true);
}

} // namespace QaplaTester
