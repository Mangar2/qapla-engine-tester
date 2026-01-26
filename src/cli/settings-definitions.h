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
#include <unordered_map>
#include <string>

namespace QaplaTester::Settings {

/// Returns key definitions for engine group
std::unordered_map<std::string, ParameterDefinition> getEngineKeys();

/// Returns key definitions for logging group
std::unordered_map<std::string, ParameterDefinition> getLoggingKeys();

/// Returns key definitions for each group
std::unordered_map<std::string, ParameterDefinition> getEachKeys();

/// Returns key definitions for epd group
std::unordered_map<std::string, ParameterDefinition> getEpdKeys();

/// Returns key definitions for sprt group
std::unordered_map<std::string, ParameterDefinition> getSprtKeys();

/// Returns key definitions for openings group
std::unordered_map<std::string, ParameterDefinition> getOpeningsKeys();

/// Returns key definitions for test group
std::unordered_map<std::string, ParameterDefinition> getTestKeys();

/// Returns key definitions for pgnoutput group
std::unordered_map<std::string, ParameterDefinition> getPgnOutputKeys();

/// Returns key definitions for tournament group
std::unordered_map<std::string, ParameterDefinition> getTournamentKeys();

/// Returns key definitions for draw group
std::unordered_map<std::string, ParameterDefinition> getDrawAdjudicationKeys();

/// Returns key definitions for resign group
std::unordered_map<std::string, ParameterDefinition> getResignAdjudicationKeys();

/// Returns key definitions for spsa group
std::unordered_map<std::string, ParameterDefinition> getSpsaKeys();

/// Returns key definitions for spsavalue group
std::unordered_map<std::string, ParameterDefinition> getSpsaValueKeys();

} // namespace QaplaTester::Settings

#endif // SETTINGS_DEFINITIONS_H
