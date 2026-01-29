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
    /**
     * @brief Saves all tournament sections from ConfigData to a file.
     * @param filename The file path to save to.
     * @param configData The configuration data containing all sections.
     * @param id The identifier for the tournament (default: "tournament").
     */
    static void save(const std::string& filename, 
                    const QaplaHelpers::ConfigData& configData,
                    const std::string& id = "tournament");

    /**
     * @brief List of all section names used in tournament files.
     * 
     */
    static constexpr std::array<const char*, 8> sectionNames = {
        "each",
        "engine",
        "tournament",
        "openings",
        "pgnoutput",
        "draw",
        "resign",
        "round"
    };
};

} // namespace QaplaTester
