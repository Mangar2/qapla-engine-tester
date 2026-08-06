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

#include "engine-parameter-bounds.h"

#include "engine-worker-factory.h"

#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"
#include "../base-elements/string-helper.h"

#include <cmath>
#include <format>

namespace QaplaTester {

namespace {

/**
 * @brief Searches an engine option by name, ignoring case.
 * @return Pointer to the option or nullptr if the engine does not support it.
 */
[[nodiscard]] const EngineOption* findOption(const EngineOptions& options, const std::string& name) {
    const auto lowerName = QaplaHelpers::to_lowercase(name);
    for (const auto& option : options) {
        if (QaplaHelpers::to_lowercase(option.name) == lowerName) {
            return &option;
        }
    }
    return nullptr;
}

/**
 * @brief Formats an optional bound for the error message.
 */
[[nodiscard]] std::string boundToString(const std::optional<int>& bound) {
    return bound ? std::to_string(*bound) : std::string("unlimited");
}

} // namespace

void validateParameterRanges(const EngineOptions& supportedOptions,
    const std::vector<OptimizerParameterRange>& ranges,
    const std::string& engineName,
    std::string_view context) {

    if (supportedOptions.empty()) {
        return;
    }

    std::string violations;
    for (const auto& range : ranges) {
        const auto* option = findOption(supportedOptions, range.name);
        if (option == nullptr) {
            Logger::reportLogger().log(
                std::format("{}: engine '{}' does not support the option '{}'. It will be ignored by the engine.",
                    context, engineName, range.name),
                TraceLevel::warning);
            continue;
        }
        if (!option->min && !option->max) {
            continue;
        }

        // The optimizers send rounded integer values, thus the rounded range is checked.
        const auto requestedMin = std::lround(range.minValue);
        const auto requestedMax = std::lround(range.maxValue);
        const bool minViolated = option->min && requestedMin < *option->min;
        const bool maxViolated = option->max && requestedMax > *option->max;
        if (!minViolated && !maxViolated) {
            continue;
        }

        violations += std::format("\n  - '{}': engine allows {}..{}, configured range is {}..{}",
            option->name,
            boundToString(option->min),
            boundToString(option->max),
            requestedMin,
            requestedMax);
    }

    if (violations.empty()) {
        return;
    }

    throw AppError::makeInvalidParameters(std::format(
        "{}: the configured optimization range is not covered by the option bounds of engine '{}':{}"
        "\nPlease reduce min/max in the configuration or widen the option bounds in the engine.",
        context, engineName, violations));
}

void validateParameterRangesAgainstEngine(const EngineConfig& engine,
    const std::vector<OptimizerParameterRange>& ranges,
    std::string_view context) {

    if (ranges.empty() || engine.getCmd().empty()) {
        return;
    }

    auto engines = EngineWorkerFactory::createEngines(engine, 1);
    if (engines.empty() || !engines.front()) {
        Logger::reportLogger().log(
            std::format("{}: engine '{}' could not be started, skipping parameter bound check.",
                context, engine.getName()),
            TraceLevel::warning);
        return;
    }

    const auto supportedOptions = engines.front()->getSupportedOptions();
    engines.clear();

    validateParameterRanges(supportedOptions, ranges, engine.getName(), context);
}

} // namespace QaplaTester
