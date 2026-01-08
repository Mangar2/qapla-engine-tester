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

#include "../opening/pgn-save.h"
#include "../base-elements/ini-file.h"

namespace QaplaTester {

/**
 * @brief Handles loading and saving of PGN output configuration from/to INI file sections.
 */
class PgnConfig {
public:
    /**
     * @brief Creates INI file sections from PgnSave::Options.
     * @param options The PGN options to convert.
     * @param id The identifier for the configuration.
     * @return Vector containing one section with PGN configuration.
     */
    static std::vector<QaplaHelpers::IniFile::Section> getSections(
        const PgnSave::Options& options, const std::string& id);

    /**
     * @brief Loads PGN configuration from INI file sections.
     * @param sections The sections containing PGN configuration.
     * @param options Reference to PgnSave::Options to populate.
     */
    static void loadFromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& sections, 
        PgnSave::Options& options);

    /**
     * @brief Saves PGN configuration sections to ConfigData.
     * @param configData The configuration data to save to.
     * @param options The PGN options to save.
     * @param id The identifier for the configuration.
     */
    static void saveToConfigData(
        QaplaHelpers::ConfigData& configData, 
        const PgnSave::Options& options, 
        const std::string& id);

    /**
     * @brief Loads PGN configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param options Reference to PgnSave::Options to populate.
     * @param id The identifier for the configuration.
     * @return true if configuration was loaded, false if not found.
     */
    static bool loadFromConfigData(
        const QaplaHelpers::ConfigData& configData, 
        PgnSave::Options& options, 
        const std::string& id);
};

} // namespace QaplaTester
