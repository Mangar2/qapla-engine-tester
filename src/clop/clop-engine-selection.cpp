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
 * @author Volker Boehm
 * @copyright Copyright (c) 2026 Volker Boehm
 */

#include "clop-engine-selection.h"

#include "../base-elements/app-error.h"

#include <algorithm>
#include <format>

namespace QaplaTester::Clop {

size_t resolveOptimizingEngineIndex(const std::vector<EngineConfig>& engines) {
    if (engines.empty()) {
        throw AppError::makeInvalidParameters("CLOP requires at least one engine.");
    }

    const auto gauntletCount = std::ranges::count_if(
        engines,
        [](const EngineConfig& engine) {
            return engine.isGauntlet();
        });

    if (gauntletCount > 1) {
        throw AppError::makeInvalidParameters(
            "CLOP supports at most one gauntlet engine.");
    }

    const auto gauntletIterator = std::ranges::find_if(
        engines,
        [](const EngineConfig& engine) {
            return engine.isGauntlet();
        });

    if (gauntletIterator != engines.end()) {
        return static_cast<size_t>(std::distance(engines.begin(), gauntletIterator));
    }

    return 0U;
}

std::vector<EngineConfig> createOpponentEngines(
    const std::vector<EngineConfig>& engines,
    size_t optimizingEngineIndex) {

    if (engines.empty()) {
        throw AppError::makeInvalidParameters("CLOP requires at least one engine.");
    }
    if (optimizingEngineIndex >= engines.size()) {
        throw AppError::makeInvalidParameters(
            std::format(
                "CLOP optimizing engine index out of range: {} (engines={}).",
                optimizingEngineIndex,
                engines.size()));
    }

    if (engines.size() == 1U) {
        return { engines.front() };
    }

    std::vector<EngineConfig> opponents;
    opponents.reserve(engines.size() - 1U);
    for (size_t index = 0; index < engines.size(); ++index) {
        if (index != optimizingEngineIndex) {
            opponents.push_back(engines[index]);
        }
    }

    if (opponents.empty()) {
        throw AppError::makeInvalidParameters("CLOP requires at least one opponent engine.");
    }

    return opponents;
}

size_t nextOpponentIndex(size_t currentIndex, size_t opponentCount) {
    if (opponentCount == 0U) {
        throw AppError::makeInvalidParameters("CLOP requires at least one opponent engine.");
    }

    if (currentIndex + 1U >= opponentCount) {
        return 0U;
    }
    return currentIndex + 1U;
}

} // namespace QaplaTester::Clop
