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
 * @copyright Copyright (c) 2026 Volker Böhm
 */
#pragma once

#include "engine-config.h"
#include "engine-option.h"

#include <string>
#include <string_view>
#include <vector>

namespace QaplaTester {

/**
 * @brief Value range of a single parameter that shall be optimized.
 */
struct OptimizerParameterRange {
    std::string name;
    double minValue;
    double maxValue;
};

/**
 * @brief Checks that the option bounds reported by an engine cover the ranges to be optimized.
 * Parameters without a matching engine option or without bounds are reported as warning only.
 * @param supportedOptions Options reported by the engine.
 * @param ranges Parameter ranges configured for the optimization.
 * @param engineName Engine name used in messages.
 * @param context Task name used in messages, e.g. "SPSA" or "CLOP".
 * @throws AppError with return code InvalidParameters if an engine bound does not cover a range.
 */
void validateParameterRanges(const EngineOptions& supportedOptions,
    const std::vector<OptimizerParameterRange>& ranges,
    const std::string& engineName,
    std::string_view context);

/**
 * @brief Starts the engine once, reads its option definitions and validates the given ranges.
 * Does nothing if the engine defines no executable or cannot be started; a failing engine is
 * reported by the regular engine startup handling.
 * @param engine Configuration of the engine to be optimized.
 * @param ranges Parameter ranges configured for the optimization.
 * @param context Task name used in messages, e.g. "SPSA" or "CLOP".
 * @throws AppError with return code InvalidParameters if an engine bound does not cover a range.
 */
void validateParameterRangesAgainstEngine(const EngineConfig& engine,
    const std::vector<OptimizerParameterRange>& ranges,
    std::string_view context);

} // namespace QaplaTester
