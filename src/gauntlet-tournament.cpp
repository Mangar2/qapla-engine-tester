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
#include "pgn-io.h"
#include "gauntlet-tournament.h"

bool GauntletTournament::wait() {
    GameManagerPool::getInstance().waitForTask(this);
    return true;
};

void GauntletTournament::run(const std::vector<EngineConfig>& engines,
    int concurrency,
    const TournamentConfig& config) {
    config_ = config;
    concurrency_ = concurrency;

    // Öffnungen einlesen
    if (config.openings.file.empty()) {
        Logger::testLogger().log("No openings file provided.", TraceLevel::error);
        return;
    }
    if (config.openings.format != "epd" && config.openings.format != "raw") {
        Logger::testLogger().log("Unsupported openings format: " + config.openings.format, TraceLevel::error);
        return;
    }

    EpdReader reader(config.openings.file);
    for (const auto& entry : reader.all()) {
        if (!entry.fen.empty()) {
            startPositions_.push_back(entry.fen);
        }
    }

    if (startPositions_.empty()) {
        Logger::testLogger().log("No valid openings found in file.", TraceLevel::error);
        return;
    }

    // Engines nach Rolle trennen
    for (const auto& engine : engines) {
        if (engine.isGauntlet()) {
            gauntletEngines_.push_back(engine);
        }
        else {
            opponentEngines_.push_back(engine);
        }
    }

    if (gauntletEngines_.empty() || opponentEngines_.empty()) {
        Logger::testLogger().log("At least one gauntlet and one opponent engine required.", TraceLevel::error);
        return;
    }

    if (config.games % config.repeat != 0) {
        Logger::testLogger().log("Warning: games is not divisible by repeat. Opening usage may be unbalanced.", 
            TraceLevel::warning);
    }

    // Spiele vorbereiten
    const int gamesPerSide = config.games / 2;
    const int openingRepeat = config.repeat;
    const int totalEncounters = config.rounds * gamesPerSide;

    for (int round = 0; round < config.rounds; ++round) {
        for (const auto& gauntlet : gauntletEngines_) {
            for (const auto& opponent : opponentEngines_) {
                for (int g = 0; g < gamesPerSide; ++g) {
                    const std::string& opening = startPositions_[(nextIndex_++) % startPositions_.size()];

                    for (int r = 0; r < openingRepeat; ++r) {
                        if (!config.noSwap) {
                            tasks_.emplace_back(gauntlet.getName(), opponent.getName(), config.tc, opening);
                            tasks_.emplace_back(opponent.getName(), gauntlet.getName(), config.tc, opening);
                        }
                        else {
                            tasks_.emplace_back(gauntlet.getName(), opponent.getName(), config.tc, opening);
                        }
                    }
                }
            }
        }
    }

    nextIndex_ = 0;
    PgnIO::tournament().initialize(config.event);
    GameManagerPool::getInstance().setConcurrency(concurrency, true);

    for (const auto& task : tasks_) {
        GameManagerPool::getInstance().addTask(this, task.white, task.black);
    }
}


