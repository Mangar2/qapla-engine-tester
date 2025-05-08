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

#include "engine-adapter-factory.h"
#include "uci-adapter.h"

std::vector<std::unique_ptr<EngineAdapter>>
EngineAdapterFactory::createUci(const std::filesystem::path& executablePath,
    std::optional<std::filesystem::path> workingDirectory,
    std::size_t count) const
{
    std::vector<std::unique_ptr<EngineAdapter>> result;
    result.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        EngineProcess proc(executablePath, workingDirectory);
        result.push_back(std::make_unique<UciAdapter>(std::move(proc)));
    }

    return result;
}