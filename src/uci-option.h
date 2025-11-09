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

#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <optional>
#include <algorithm>
#include <stdexcept>

#include "engine-option.h"

namespace QaplaTester {

/**
 * @brief Parses a single UCI option line and returns a UciOption.
 *        Throws std::runtime_error on malformed input.
 *        Example line: "option name Hash type spin default 128 min 1 max 4096"
 */
EngineOption parseUciOptionLine(const std::string& line);

} // namespace QaplaTester
