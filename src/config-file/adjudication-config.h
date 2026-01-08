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
 * @brief Handles loading and saving of adjudication configurations from/to INI file sections.
 * 
 * This includes both draw and resign adjudication configurations.
 */
class AdjudicationConfig {
public:
    /**
     * @brief Creates INI file sections from adjudication configurations.
     * @param drawConfig The draw adjudication configuration.
     * @param resignConfig The resign adjudication configuration.
     * @param id The identifier for the configuration.
     * @return Vector containing two sections: drawadjudication and resignadjudication.
     */
    static std::vector<QaplaHelpers::IniFile::Section> getSections(
        const AdjudicationManager::DrawAdjudicationConfig& drawConfig,
        const AdjudicationManager::ResignAdjudicationConfig& resignConfig,
        const std::string& id);

    /**
     * @brief Loads adjudication configurations from INI file sections.
     * @param drawSections The sections containing draw adjudication configuration.
     * @param resignSections The sections containing resign adjudication configuration.
     * @param drawConfig Reference to DrawAdjudicationConfig to populate.
     * @param resignConfig Reference to ResignAdjudicationConfig to populate.
     */
    static void loadFromSections(
        const std::vector<QaplaHelpers::IniFile::Section>& drawSections,
        const std::vector<QaplaHelpers::IniFile::Section>& resignSections,
        AdjudicationManager::DrawAdjudicationConfig& drawConfig,
        AdjudicationManager::ResignAdjudicationConfig& resignConfig);

    /**
     * @brief Saves adjudication configuration sections to ConfigData.
     * @param configData The configuration data to save to.
     * @param drawConfig The draw adjudication configuration.
     * @param resignConfig The resign adjudication configuration.
     * @param id The identifier for the configuration.
     */
    static void saveToConfigData(
        QaplaHelpers::ConfigData& configData,
        const AdjudicationManager::DrawAdjudicationConfig& drawConfig,
        const AdjudicationManager::ResignAdjudicationConfig& resignConfig,
        const std::string& id);

    /**
     * @brief Loads adjudication configurations from ConfigData.
     * @param configData The configuration data to load from.
     * @param drawConfig Reference to DrawAdjudicationConfig to populate.
     * @param resignConfig Reference to ResignAdjudicationConfig to populate.
     * @param id The identifier for the configuration.
     * @return true if at least one configuration was loaded, false if not found.
     */
    static bool loadFromConfigData(
        const QaplaHelpers::ConfigData& configData,
        AdjudicationManager::DrawAdjudicationConfig& drawConfig,
        AdjudicationManager::ResignAdjudicationConfig& resignConfig,
        const std::string& id);
};

} // namespace QaplaTester
