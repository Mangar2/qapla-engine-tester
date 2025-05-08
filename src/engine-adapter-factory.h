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

#include <memory>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include "engine-adapter.h"

 /**
  * @brief Factory for creating EngineAdapter instances based on engine type.
  */
class EngineAdapterFactory {
public:
    /**
     * @brief Attempts to create an EngineAdapter for the given executable path.
     * @param executablePath Full path to the engine binary.
     * @param workingDirectory Optional working directory for the engine process.
     * @param options Optional startup options to be applied after initialization.
     * @return A ready-to-use EngineAdapter instance.
     */
    std::vector<std::unique_ptr<EngineAdapter>>
        createUci(const std::filesystem::path& executablePath,
            std::optional<std::filesystem::path> workingDirectory = std::nullopt,
            std::size_t count = 1) const;
};
