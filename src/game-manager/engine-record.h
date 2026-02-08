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


#include <vector>
#include <optional>

#include "../chess-game/move-record.h"
#include "../engine-handling/engine-config.h"
#include "../engine-handling/engine-option.h"

namespace QaplaTester {

struct EngineRecord {
    std::string identifier;
    EngineConfig config;
    std::optional<EngineOptions> supportedOptions;
    enum class Status: std::uint8_t {
        NotStarted,
        Starting,
        Running,
        Error
    } status = Status::NotStarted;
    std::optional<size_t> memoryUsageB;
    static std::string to_string(Status status) {
        switch (status) {
        case Status::NotStarted: return "Not Started";
        case Status::Starting: return "Starting";
        case Status::Running: return "Running";
        case Status::Error: return "Error";
		default: return "Unknown";
        }
	}
};

using EngineRecords = std::vector<EngineRecord>;

} // namespace QaplaTester
