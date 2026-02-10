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

#include "settings-manager.h"
#include <string>

/**
 * @brief Helper functions for configuring engines from settings.
 */
namespace QaplaTester::Settings::Helper {

    /**
     * @brief Applies engine configuration from the settings manager.
     * @param manager The settings manager to read from.
     * @param groupName The group name for individual engine settings.
     * @return The rapid mode flag value.
     */
    bool applyEngineSettings(Manager& manager, const std::string& groupName);

    /**
     * @brief Adds engines to the active list from a comma-separated string of configuration names.
     * @param engineNamesStr Comma-separated list of engine configuration names.
     */
    void addEnginesFromList(const std::string& engineNamesStr);


} // namespace QaplaTester::Settings::Helper
