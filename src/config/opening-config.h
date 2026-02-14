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

#include "../opening/openings.h"
#include "../cli/settings-manager.h"

namespace QaplaTester {

/**
 * @brief Handles loading and saving of opening configuration from/to INI file sections.
 * 
 * This class provides static methods to convert between Openings data structures
 * and INI file sections, allowing both GUI and CLI to use the same format.
 */
class OpeningConfig {
public:
    /**
     * @brief Gets the section name for opening configuration.
     * @return The section name used in INI files.
     */
    [[nodiscard]] static constexpr const char* getSectionName() { return "openings"; }

    /**
     * @brief Creates opening configuration from Settings::Manager.
     * @param manager The settings manager to read from.
     * @param groupName The group instance name.
     * @return Openings structure populated from manager.
     */
    [[nodiscard]] static Openings fromManager(
        Settings::Manager& manager,
        const std::string& groupName);
};

} // namespace QaplaTester
