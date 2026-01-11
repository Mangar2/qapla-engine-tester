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
#include <string>

namespace QaplaTester {

/**
 * @brief Global engine configuration that applies to all engines.
 * 
 * This structure contains global settings that can be selectively applied to all engines.
 * Each setting has a corresponding "use" flag to indicate whether it should override
 * engine-specific settings.
 */
struct EngineGlobalConfig {
    bool useGlobalHash = true;          ///< Whether to use global hash size
    uint32_t hashSizeMB = 32;           ///< Hash size in MB (1-64000)
    
    bool useGlobalPonder = true;        ///< Whether to use global ponder setting
    bool ponder = false;                ///< Whether pondering is enabled globally
    
    bool useGlobalTrace = true;         ///< Whether to use global trace level
    std::string traceLevel = "command"; ///< Global trace level ("none", "all", "command")
    
    bool useGlobalRestart = true;       ///< Whether to use global restart option
    std::string restart = "auto";       ///< Global restart option ("auto", "on", "off")
    
    std::string timeControl = "60.0+0.0"; ///< Time control string (always applied)
};

/**
 * @brief Handles loading and saving of global engine configuration from/to INI file sections.
 * 
 * This class manages global settings that apply to all engines, and provides functionality
 * to merge these global settings with engine-specific configurations.
 */
class EngineGlobalConfigFile {
public:
    /**
     * @brief Gets the section name for global engine configuration.
     * @return The section name used in INI files.
     */
    [[nodiscard]] static constexpr const char* getSectionName() { return "eachengine"; }

    /**
     * @brief Creates INI file section from EngineGlobalConfig.
     * @param config The global engine configuration to convert.
     * @param id The identifier for the configuration.
     * @return INI file section containing global engine configuration.
     */
    [[nodiscard]] static QaplaHelpers::IniFile::Section toSection(
        const EngineGlobalConfig& config,
        const std::string& id);

    /**
     * @brief Creates global engine configuration from INI file section.
     * @param section The section containing global engine configuration.
     * @return EngineGlobalConfig populated from section.
     */
    [[nodiscard]] static EngineGlobalConfig fromSection(
        const QaplaHelpers::IniFile::Section& section);

    /**
     * @brief Creates INI file sections for engine configuration from EngineGlobalConfig.
     * @param config The global engine configuration to convert.
     * @param id The identifier for the configuration.
     * @return Vector containing the "eachengine" section.
     */
    [[nodiscard]] static std::vector<QaplaHelpers::IniFile::Section> toEngineConfigSections(
        const EngineGlobalConfig& config, 
        const std::string& id);

    /**
     * @brief Creates INI file sections for time control options from EngineGlobalConfig.
     * @param config The global engine configuration to convert.
     * @param id The identifier for the configuration.
     * @return Vector containing the "timecontroloptions" section.
     */
    [[nodiscard]] static std::vector<QaplaHelpers::IniFile::Section> toTimeControlSections(
        const EngineGlobalConfig& config, 
        const std::string& id);

    /**
     * @brief Loads global engine configuration from INI file sections.
     * @param sections The sections containing global engine configuration.
     * @return EngineGlobalConfig populated from sections.
     */
    [[nodiscard]] static EngineGlobalConfig fromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& sections);

    /**
     * @brief Loads global engine configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return EngineGlobalConfig if found, std::nullopt otherwise.
     */
    [[nodiscard]] static std::optional<EngineGlobalConfig> fromConfigData(
        const QaplaHelpers::ConfigData& configData, 
        const std::string& id);

    /**
     * @brief Applies global configuration to an engine configuration.
     * 
     * This function merges global settings with engine-specific settings.
     * Global settings only override engine settings if their corresponding "use" flag is true.
     * 
     * @param engine The engine configuration to modify (receives merged configuration).
     * @param globalConfig The global configuration to apply.
     */
    static void applyGlobalConfig(
        EngineConfig& engine,
        const EngineGlobalConfig& globalConfig);

private:
    /**
     * @brief Loads time control from timecontroloptions section.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return Time control string if found, std::nullopt otherwise.
     */
    static std::optional<std::string> loadTimeControl(
        const QaplaHelpers::ConfigData& configData,
        const std::string& id);

    /**
     * @brief Creates timecontroloptions section from time control.
     * @param config The configuration containing time control.
     * @param id The identifier for the configuration.
     * @return Section containing time control options.
     */
    static QaplaHelpers::IniFile::Section createTimeControlSection(
        const EngineGlobalConfig& config,
        const std::string& id);
};

} // namespace QaplaTester
