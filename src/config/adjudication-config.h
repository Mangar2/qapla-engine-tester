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
#include "../cli/settings-manager.h"

namespace QaplaTester {

/**
 * @brief Transforms adjudication configurations between internal structs and INI file sections.
 */
class AdjudicationConfig {
public:
    /**
     * @brief Gets the section name for draw adjudication configuration.
     * @return The section name used in INI files.
     */
    [[nodiscard]] static constexpr const char* getDrawSectionName() { return "draw"; }

    /**
     * @brief Gets the section name for resign adjudication configuration.
     * @return The section name used in INI files.
     */
    [[nodiscard]] static constexpr const char* getResignSectionName() { return "resign"; }

    /**
     * @brief Creates draw adjudication configuration from Settings::Manager.
     * @param manager The settings manager to read from.
     * @param groupName The group instance name.
     * @return Draw adjudication configuration populated from manager.
     */
    [[nodiscard]] static AdjudicationManager::DrawAdjudicationConfig fromDrawManager(
        Settings::Manager& manager,
        const std::string& groupName);

    /**
     * @brief Creates resign adjudication configuration from Settings::Manager.
     * @param manager The settings manager to read from.
     * @param groupName The group instance name.
     * @return Resign adjudication configuration populated from manager.
     */
    [[nodiscard]] static AdjudicationManager::ResignAdjudicationConfig fromResignManager(
        Settings::Manager& manager,
        const std::string& groupName);
};

} // namespace QaplaTester
