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

#include "../game-manager/adjudication-manager.h"
#include "../base-elements/ini-file.h"

namespace QaplaTester {

/**
 * @brief Transforms adjudication configurations between internal structs and INI file sections.
 */
class AdjudicationConfig {
public:
    /**
     * @brief Creates draw adjudication section from configuration.
     * @param config The draw adjudication configuration.
     * @param id The identifier for the configuration.
     * @return INI file section containing draw adjudication configuration.
     */
    static QaplaHelpers::IniFile::Section toDrawSection(
        const AdjudicationManager::DrawAdjudicationConfig& config,
        const std::string& id);

    /**
     * @brief Creates resign adjudication section from configuration.
     * @param config The resign adjudication configuration.
     * @param id The identifier for the configuration.
     * @return INI file section containing resign adjudication configuration.
     */
    static QaplaHelpers::IniFile::Section toResignSection(
        const AdjudicationManager::ResignAdjudicationConfig& config,
        const std::string& id);

    /**
     * @brief Loads draw adjudication configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return Draw adjudication configuration if found, std::nullopt otherwise.
     */
    static std::optional<AdjudicationManager::DrawAdjudicationConfig> fromDrawConfigData(
        const QaplaHelpers::ConfigData& configData,
        const std::string& id);

    /**
     * @brief Loads resign adjudication configuration from ConfigData.
     * @param configData The configuration data to load from.
     * @param id The identifier for the configuration.
     * @return Resign adjudication configuration if found, std::nullopt otherwise.
     */
    static std::optional<AdjudicationManager::ResignAdjudicationConfig> fromResignConfigData(
        const QaplaHelpers::ConfigData& configData,
        const std::string& id);
};

} // namespace QaplaTester
