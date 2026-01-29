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
     * @brief Gets the section name for tournament configuration.
     * @return The section name used in INI files.
     */
    [[nodiscard]] static constexpr const char* getSectionName() { return "tournament"; }

    /**
     * @brief Creates INI file sections from TournamentConfig.
     * @param config The tournament configuration to convert.
     * @param id The identifier for the configuration.
     * @return Vector containing one section with tournament configuration.
     */
    [[nodiscard]] static std::vector<QaplaHelpers::IniFile::Section> toSections(
        const TournamentConfig& config, const std::string& id);

    /**
     * @brief Loads tournament configuration from INI file sections.
     * @param sections The sections containing tournament configuration.
     * @return TournamentConfig populated from sections.
     */
    [[nodiscard]] static TournamentConfig fromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& sections);

    /**
     * @brief Loads tournament configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return TournamentConfig if found, std::nullopt otherwise.
     */
    [[nodiscard]] static std::optional<TournamentConfig> fromConfigData(
        const QaplaHelpers::ConfigData& configData, 
        const std::string& id);
};

} // namespace QaplaTester
