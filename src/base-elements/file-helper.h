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

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <set>

namespace QaplaHelpers {

/**
 * @brief Validates if the given path is a valid output file path.
 * 
 * Verifies that the path is not empty, not a directory, that the parent
 * directory exists and is writable, and that the filename follows system rules.
 * 
 * @param path The path to validate.
 * @throws QaplaTester::AppError if the path is invalid or unwritable.
 */
void validateOutputPath(const std::filesystem::path& path);

} // namespace QaplaHelpers

