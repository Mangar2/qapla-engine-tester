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

#include "../tournament/tournament.h"
#include "../base-elements/ini-file.h"

namespace QaplaTester {

/**
 * @brief Handles loading and saving of tournament configuration from/to INI file sections.
 */
class TournamentConfigFile {
public:
    /**
     * @brief Creates INI file sections from TournamentConfig.
     * @param config The tournament configuration to convert.
     * @param id The identifier for the configuration.
     * @return Vector containing one section with tournament configuration.
     */
    static std::vector<QaplaHelpers::IniFile::Section> getSections(
        const TournamentConfig& config, const std::string& id);

    /**
     * @brief Loads tournament configuration from INI file sections.
     * @param sections The sections containing tournament configuration.
     * @param config Reference to TournamentConfig to populate.
     */
    static void loadFromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& sections, 
        TournamentConfig& config);

    /**
     * @brief Saves tournament configuration sections to ConfigData.
     * @param configData The configuration data to save to.
     * @param config The tournament configuration to save.
     * @param id The identifier for the configuration.
     */
    static void saveToConfigData(
        QaplaHelpers::ConfigData& configData, 
        const TournamentConfig& config, 
        const std::string& id);

    /**
     * @brief Loads tournament configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param config Reference to TournamentConfig to populate.
     * @param id The identifier for the configuration.
     * @return true if configuration was loaded, false if not found.
     */
    static bool loadFromConfigData(
        const QaplaHelpers::ConfigData& configData, 
        TournamentConfig& config, 
        const std::string& id);
};

} // namespace QaplaTester
