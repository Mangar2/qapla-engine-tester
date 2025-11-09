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
#include <optional>

namespace QaplaTester {

 /**
  * @brief Configuration for loading and selecting opening positions.
  */
struct Openings {
    std::string file;
    std::string format = "raw";
    std::string order = "sequential";
    std::optional<int> plies;
    uint32_t start = 0;
    uint32_t seed = 815;
    std::string policy = "default";
};

} // namespace QaplaTester
