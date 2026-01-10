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
     * @brief Creates draw adjudication configuration from section.
     * @param section The INI file section containing draw adjudication configuration.
     * @return Draw adjudication configuration.
     */
    static AdjudicationManager::DrawAdjudicationConfig fromDrawSection(
        const QaplaHelpers::IniFile::Section& section);

    /**
     * @brief Creates resign adjudication configuration from section.
     * @param section The INI file section containing resign adjudication configuration.
     * @return Resign adjudication configuration.
     */
    static AdjudicationManager::ResignAdjudicationConfig fromResignSection(
        const QaplaHelpers::IniFile::Section& section);
};

} // namespace QaplaTester
