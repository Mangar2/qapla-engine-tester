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

#include "sprt-manager.h"
#include "../base-elements/ini-file.h"
#include "../engine-handling/engine-config.h"
#include <string>
#include <array>
#include <vector>

namespace QaplaTester::Settings {
    class Manager;
}

namespace QaplaTester {

/**
 * @brief Handles loading and saving of complete SPRT tournament files.
 * 
 * This class manages the complete SPRT tournament configuration file format,
 * combining all section types: engine selection, opening, pgn output, adjudication,
 * global settings, SPRT config, and tournament results.
 * 
 * It provides a central interface that both the GUI and CLI can use to
 * save and load SPRT tournament configurations in a consistent format.
 */
class SprtTournamentFile {
public:
    static constexpr const char* id = "sprt-tournament";
    /**
     * @brief Saves all SPRT tournament sections from ConfigData to a file.
     * @param filename The file path to save to.
     * @param configData The configuration data containing all sections.
     * @param id The identifier for the tournament (default: "sprt-tournament").
     */
    static void save(
        const std::string& filename, 
        const QaplaHelpers::ConfigData& configData,
        const std::string& id = SprtTournamentFile::id);

    /**
     * @brief Saves all SPRT tournament sections from SettingsManager and SprtManager to a file.
     * @param filename The file path to save to.
     * @param settingsManager The settings manager containing all configurations.
     * @param sprtManager The SPRT manager instance to get tournament results from.
     * @param id The identifier for the tournament (default: "sprt-tournament").
     */
    static void save(
        const std::string& filename, 
        const Settings::Manager& settingsManager,
        const std::shared_ptr<SprtManager>& sprtManager,
        const std::string& id = SprtTournamentFile::id);

    /**
     * @brief Sets up an autosave callback for the SPRT manager.
     * @param filename The file path to save to.
     * @param saveInterval The number of games between autosaves.
     * @param manager The SPRT manager instance.
     */
    static void setSaveCallback(
        const std::string& filename, 
        uint32_t saveInterval, 
        const std::shared_ptr<SprtManager>& manager,
        const std::vector<EngineConfig>& engines);

    /**
     * @brief Loads game results from a SPRT tournament file.
     * @param filename The file path to load from.
     * @param manager The SPRT manager instance to load results into.
     * @param id The identifier for the tournament (default: "sprt-tournament").
     */
    static void loadGameResults(
        const std::string& filename, 
        const std::shared_ptr<SprtManager>& manager,
        const std::string& id = SprtTournamentFile::id);

private:
    /**
     * @brief List of all section names used in SPRT tournament files.
     * 
     */
    static constexpr std::array<const char*, 8> sectionNames = {
        "each",
        "engine",
        "sprt",
        "openings",
        "pgnoutput",
        "draw",
        "resign",
        "round"
    };
};

} // namespace QaplaTester
