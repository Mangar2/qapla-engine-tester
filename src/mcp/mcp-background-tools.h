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

#include "../base-elements/json-helper.h"

#include <string>

namespace QaplaTester::Mcp {

/**
 * @brief Evaluates whether a tool-call argument object requests background execution.
 * @param arguments The MCP tool arguments object.
 * @return True when background execution is requested.
 */
[[nodiscard]] bool isBackgroundRequested(const JsonValue::Object& arguments);

/**
 * @brief Creates control status JSON with runner and queue information.
 * @return Serialized JSON status text.
 */
[[nodiscard]] std::string createCombinedControlStatus();

} // namespace QaplaTester::Mcp
