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
#include "../cli/settings-manager.h"

namespace QaplaTester {

/**
 * @brief Handles loading and saving of SPRT configuration from/to INI file sections.
 */
class SprtConfigFile {
public:
    /**
     * @brief Gets the section name for SPRT configuration.
     * @return The section name used in INI files.
     */
    [[nodiscard]] static constexpr const char* getSectionName() { return "sprtconfig"; }

    /**
     * @brief Creates INI file sections from SprtConfig.
     * @param config The SPRT configuration to convert.
     * @param id The identifier for the configuration.
     * @return Vector containing one section with SPRT configuration.
     */
    [[nodiscard]] static std::vector<QaplaHelpers::IniFile::Section> toSections(
        const SprtConfig& config, const std::string& id);

    /**
     * @brief Loads SPRT configuration from INI file sections.
     * @param sections The sections containing SPRT configuration.
     * @return SprtConfig populated from sections.
     */
    [[nodiscard]] static SprtConfig fromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& sections);

    /**
     * @brief Loads SPRT configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return SprtConfig if found, std::nullopt otherwise.
     */
    [[nodiscard]] static std::optional<SprtConfig> fromConfigData(
        const QaplaHelpers::ConfigData& configData, 
        const std::string& id);

    /**
     * @brief Creates SPRT configuration from Settings::Manager.
     * @param manager The settings manager to read from.
     * @param groupName The group instance name.
     * @return SprtConfig populated from manager.
     */
    [[nodiscard]] static SprtConfig fromManager(
        Settings::Manager& manager,
        const std::string& groupName);
};

} // namespace QaplaTester
