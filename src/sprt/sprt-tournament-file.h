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

#include "../base-elements/ini-file.h"
#include <string>
#include <array>

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
    /**
     * @brief Saves all SPRT tournament sections from ConfigData to a file.
     * @param filename The file path to save to.
     * @param configData The configuration data containing all sections.
     * @param id The identifier for the tournament (default: "sprt-tournament").
     */
    static void save(const std::string& filename, 
                    const QaplaHelpers::ConfigData& configData,
                    const std::string& id = "sprt-tournament");

    /**
     * @brief Loads all SPRT tournament sections from a file into ConfigData.
     * @param filename The file path to load from.
     * @param configData The configuration data to populate.
     * @param id The identifier for the tournament (default: "sprt-tournament").
     */
    static void load(const std::string& filename, 
                    QaplaHelpers::ConfigData& configData,
                    const std::string& id = "sprt-tournament");

    /**
     * @brief Helper method to load tournament settings form an sprt-tournament-file.
     * 
     * This is that loads the tournament file and changes all the settings based 
     * on its contents.
     * 
     * @param filename The file path to load from.
     * @param configData The configuration data to populate.
     * @param id The identifier for the tournament (default: "sprt-tournament").
     * @return true if state was loaded successfully, false otherwise.
     */
    static bool loadSprtSettings(const std::string& filename,
                               QaplaHelpers::ConfigData& configData,
                               const std::string& id = "sprt-tournament");

    /**
     * @brief Helper method to load tournament state from ConfigData into a SprtManager.
     * 
     * This is a convenience method that uses existing ConfigData and
     * applies the "round" section to the manager's internal state.
     * 
     * @param configData The configuration data containing tournament state.
     * @param manager The SprtManager to load state into.
     * @param id The identifier for the tournament (default: "sprt-tournament").
     * @return true if state was loaded successfully, false otherwise.
     */
    static bool loadIntoManagerFromConfigData(const QaplaHelpers::ConfigData& configData,
                                             class SprtManager& manager,
                                             const std::string& id = "sprt-tournament");

    /**
     * @brief List of all section names used in SPRT tournament files.
     * 
     * These sections include:
     * - eachengine: Global engine settings
     * - engineselection: Selected engines and their configurations
     * - sprtconfig: SPRT test parameters
     * - opening: Opening book configuration
     * - pgnoutput: PGN output options
     * - drawadjudication: Draw adjudication settings
     * - resignadjudication: Resign adjudication settings
     * - timecontroloptions: Time control settings
     * - round: Tournament results (game records)
     */
    static constexpr std::array<const char*, 9> sectionNames = {
        "eachengine",
        "engineselection",
        "sprtconfig",
        "opening",
        "pgnoutput",
        "drawadjudication",
        "resignadjudication",
        "timecontroloptions",
        "round"
    };
};

} // namespace QaplaTester
