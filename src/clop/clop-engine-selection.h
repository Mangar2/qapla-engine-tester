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

#pragma once

#include "../engine-handling/engine-config.h"

#include <cstddef>
#include <vector>

namespace QaplaTester::Clop {

/**
 * @brief Resolves the index of the optimizing engine.
 * @param engines Configured engines for CLOP.
 * @return Index of the optimizing engine.
 * @throws AppError InvalidParameters when no engines exist or multiple gauntlets are set.
 */
[[nodiscard]] size_t resolveOptimizingEngineIndex(const std::vector<EngineConfig>& engines);

/**
 * @brief Builds the ordered opponent list for CLOP sample scheduling.
 * @param engines Configured engines for CLOP.
 * @param optimizingEngineIndex Index of the optimizing engine.
 * @return Opponent list in stable input order.
 * @throws AppError InvalidParameters if arguments are invalid.
 */
[[nodiscard]] std::vector<EngineConfig> createOpponentEngines(
    const std::vector<EngineConfig>& engines,
    size_t optimizingEngineIndex);

/**
 * @brief Computes the next opponent index in round-robin order.
 * @param currentIndex Current opponent index.
 * @param opponentCount Number of available opponents.
 * @return Next opponent index.
 * @throws AppError InvalidParameters when opponentCount is zero.
 */
[[nodiscard]] size_t nextOpponentIndex(size_t currentIndex, size_t opponentCount);

} // namespace QaplaTester::Clop
