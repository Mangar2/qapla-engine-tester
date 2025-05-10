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
#include <memory>
#include <functional>
#include "engine-worker.h"

 /**
  * @brief Groups multiple EngineWorker instances for batch control.
  *
  * EngineGroup allows coordinated operations on multiple workers,
  * such as broadcasting commands or querying state in parallel setups.
  */
class EngineGroup {
public:
    /**
     * @brief Constructs an engine group from a list of EngineWorker instances.
     * @param workers Vector of initialized EngineWorker objects.
     */
    explicit EngineGroup(std::vector<std::unique_ptr<EngineWorker>> workers)
        : engines_(std::move(workers)) {
    }

    /**
     * @brief Applies a function to each EngineWorker in the group.
     * @param fn Function taking EngineWorker& as parameter.
     */
    void forEach(const std::function<void(EngineWorker&)>& fn) {
        for (auto& engine : engines_) {
            fn(*engine);
        }
    }

    /**
     * @brief Returns the number of workers in the group.
     */
    std::size_t size() const {
        return engines_.size();
    }

    std::vector<std::unique_ptr<EngineWorker>> engines_;
};
