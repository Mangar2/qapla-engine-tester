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

namespace QaplaTester {

/**
 * @brief Handles loading and saving of SPRT configuration from/to INI file sections.
 */
class SprtConfigFile {
public:
    /**
     * @brief Creates INI file sections from SprtConfig.
     * @param config The SPRT configuration to convert.
     * @param id The identifier for the configuration.
     * @return Vector containing one section with SPRT configuration.
     */
    static std::vector<QaplaHelpers::IniFile::Section> getSections(
        const SprtConfig& config, const std::string& id);

    /**
     * @brief Loads SPRT configuration from INI file sections.
     * @param sections The sections containing SPRT configuration.
     * @return SprtConfig populated from sections.
     */
    static SprtConfig loadFromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& sections);

    /**
     * @brief Loads SPRT configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return SprtConfig if found, std::nullopt otherwise.
     */
    static std::optional<SprtConfig> loadFromConfigData(
        const QaplaHelpers::ConfigData& configData, 
        const std::string& id);
};

} // namespace QaplaTester
