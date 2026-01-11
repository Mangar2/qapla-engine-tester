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

#include "../engine-handling/engine-config.h"
#include "../base-elements/ini-file.h"
#include <optional>

namespace QaplaTester {

/**
 * @brief Engine configuration with metadata for GUI purposes.
 */
struct EngineConfiguration {
    EngineConfig config;
    std::string originalName;
    bool selected = false;
};

/**
 * @brief Handles loading and saving of engine-specific configuration from/to INI file sections.
 * 
 * This class manages engine-specific settings that override or extend global engine settings.
 * It transforms between EngineConfig objects and INI file sections for individual engines.
 */
class EngineConfigFile {
public:
    /**
     * @brief Creates INI file section from EngineConfig.
     * @param config The engine configuration to convert.
     * @return INI file section containing engine configuration.
     */
    static QaplaHelpers::IniFile::Section toSection(const EngineConfig& config);

    /**
     * @brief Loads engine configuration from INI file section.
     * @param section The section containing engine configuration.
     * @return EngineConfiguration populated from section.
     */
    static EngineConfiguration fromSection(const QaplaHelpers::IniFile::Section& section);

    /**
     * @brief Creates INI file sections from multiple EngineConfigs.
     * @param configs Vector of engine configurations to convert.
     * @return Vector of INI file sections containing engine configurations.
     */
    static std::vector<QaplaHelpers::IniFile::Section> getSections(
        const std::vector<EngineConfig>& configs);

    /**
     * @brief Loads multiple engine configurations from INI file sections.
     * @param sections The sections containing engine configurations.
     * @return Vector of EngineConfig objects populated from sections.
     */
    static std::vector<EngineConfig> fromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& sections);

    /**
     * @brief Loads engine configurations from ConfigData.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return Vector of EngineConfig if found, std::nullopt otherwise.
     */
    static std::optional<std::vector<EngineConfig>> fromConfigData(
        const QaplaHelpers::ConfigData& configData, 
        const std::string& id);
};

} // namespace QaplaTester
