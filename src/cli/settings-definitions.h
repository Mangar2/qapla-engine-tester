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

#ifndef SETTINGS_DEFINITIONS_H
#define SETTINGS_DEFINITIONS_H

#include "settings-manager.h"
#include "../base-elements/stable-map.h"
#include <string>

namespace QaplaTester::Settings {

/// Returns key definitions for engine group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getEngineKeys();

/// Returns key definitions for logging group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getLoggingKeys();

/// Returns key definitions for mcp group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getMcpKeys();

/// Returns key definitions for each group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getEachKeys();

/// Returns key definitions for epd group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getEpdKeys();

/// Returns key definitions for sprt group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getSprtKeys();

/// Returns key definitions for openings group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getOpeningsKeys();

/// Returns key definitions for test group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getTestKeys();

/// Returns key definitions for systemtest group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getSystemTestKeys();

/// Returns key definitions for pgnoutput group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getPgnOutputKeys();

/// Returns key definitions for tournament group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getTournamentKeys();

/// Returns key definitions for draw group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getDrawAdjudicationKeys();

/// Returns key definitions for resign group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getResignAdjudicationKeys();

/// Returns key definitions for spsa group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getSpsaKeys();

/// Returns key definitions for spsavalue group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getSpsaValueKeys();

/// Returns key definitions for clop group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getClopKeys();

/// Returns key definitions for clopvalue group
QaplaHelpers::StableMap<std::string, ParameterDefinition> getClopValueKeys();

/**
 * @brief Initializes all CLI settings by registering them with the Manager
 * 
 */
void initSettings();

} // namespace QaplaTester::Settings

#endif // SETTINGS_DEFINITIONS_H
