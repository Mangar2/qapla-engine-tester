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

#pragma once

#include "tournament.h"
#include "../base-elements/ini-file.h"
#include "../engine-handling/engine-config.h"
#include <string>
#include <array>
#include <memory>
#include <vector>

namespace QaplaTester::Settings {
    class Manager;
}

namespace QaplaTester {

/**
 * @brief Handles loading and saving of complete tournament files.
 * 
 * This class manages the complete tournament configuration file format,
 * combining all section types: engine selection, tournament config, opening, 
 * pgn output, adjudication, global settings, and tournament results.
 * 
 * It provides a central interface that both the GUI and CLI can use to
 * save and load tournament configurations in a consistent format.
 */
class TournamentFile {
public:
    static constexpr const char* id = "tournament";

    /**
     * @brief Saves all tournament sections from ConfigData to a file.
     * @param filename The file path to save to.
     * @param configData The configuration data containing all sections.
     * @param id The identifier for the tournament (default: "tournament").
     */
    static void save(const std::string& filename, 
                    const QaplaHelpers::ConfigData& configData,
                    const std::string& id = TournamentFile::id);

    /**
     * @brief Saves all tournament sections from SettingsManager and Tournament to a file.
     * @param filename The file path to save to.
     * @param settingsManager The settings manager containing all configurations.
     * @param tournament The tournament instance to get results from.
     * @param id The identifier for the tournament (default: "tournament").
     */
    static void save(const std::string& filename, 
                    const Settings::Manager& settingsManager,
                    const std::shared_ptr<Tournament>& tournament,
                    const std::string& id = TournamentFile::id);

    /**
     * @brief Sets up an autosave callback for the tournament.
     * @param filename The file path to save to.
     * @param saveIntervalMs The number of milliseconds between autosaves.
     * @param tournament The tournament instance.
     */
    static void setSaveCallback(const std::string& filename, uint32_t saveIntervalMs, 
                                const std::shared_ptr<Tournament>& tournament,
                                const std::vector<EngineConfig>& engines);
    /**
     * @brief Loads game results from a tournament file into a tournament instance.
     * @param filename The file path to load from.
     * @param tournament The tournament instance to load results into.
     * @param id The identifier for the tournament (default: "tournament").
     */
    static void loadGameResults(const std::string& filename, 
                               const std::shared_ptr<Tournament>& tournament, 
                               const std::string& id = TournamentFile::id);

    /**
     * @brief List of all section names used in tournament files.
     * 
     */
    static constexpr std::array<const char*, 9> sectionNames = {
        "each",
        "engine",
        "tournament",
        "openings",
        "pgnoutput",
        "draw",
        "resign",
        "global",
        "round"
    };

private:
    /**
     * @brief Creates ConfigData for tournament configuration sections.
     * Excludes runtime-specific sections that are merged separately.
     * @param settingsManager The settings manager used as source.
     * @return ConfigData containing configured sections except engine and round.
     */
    [[nodiscard]] static QaplaHelpers::ConfigData getConfigData(
            const Settings::Manager& settingsManager);
};

} // namespace QaplaTester
