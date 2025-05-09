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
#include "engine-adapter.h"

class EngineGroup {
public:
    explicit EngineGroup(std::vector<std::unique_ptr<EngineAdapter>> adapters)
        : engines_(std::move(adapters)) {
    }

    void forEach(const std::function<void(EngineAdapter&)>& fn) {
        for (auto& engine : engines_) {
            fn(*engine);
        }
    }

    std::size_t size() const {
        return engines_.size();
    }

    EngineAdapter& operator[](std::size_t index) {
        return *engines_.at(index);
    }

private:
    std::vector<std::unique_ptr<EngineAdapter>> engines_;
};
