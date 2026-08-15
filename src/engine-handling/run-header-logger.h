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

#include "engine-config.h"

#include <string>
#include <string_view>
#include <vector>

namespace QaplaTester {

/**
 * @brief A single labeled setting printed in a run header.
 *
 * The value is always a string and, where meaningful, already contains its
 * unit (e.g. key "H0", value "5 elo").
 */
struct RunHeaderSetting {
    std::string key;
    std::string value;
};

/**
 * @brief Writes a uniform header to the report log at the start of a run.
 *
 * Used by SPRT, Tournament, EPD, CLOP, SPSA and Engine-Test to print the
 * same kind of "first lines" for every run: which engines participate (with
 * their configuration) and which settings apply to this run. Content further
 * down in the log (result tables, per-game lines, checklists, ...) is
 * written by the respective run type itself and is not affected by this
 * class.
 */
class RunHeaderLogger {
public:
    /**
     * @brief Logs the run header to the report logger.
     * @param runType Human-readable name of the run (e.g. "SPRT", "Tournament", "EPD", "CLOP", "SPSA", "Engine Test").
     * @param engines Engine configurations participating in this run (may be empty, e.g. for a Monte Carlo simulation).
     * @param settings Ordered run-specific settings printed as key/value lines.
     */
    static void log(std::string_view runType,
        const std::vector<EngineConfig>& engines,
        const std::vector<RunHeaderSetting>& settings);

private:
    static void logEngine(size_t index, const EngineConfig& engine);
};

} // namespace QaplaTester
