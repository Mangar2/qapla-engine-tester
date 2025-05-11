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

#include <optional>
#include "engine-adapter.h"

/**
 * Represents time control constraints for engine testing and can produce GoLimits.
 */
class TimeControl {
public:
    void setMoveTime(int64_t ms) { movetimeMs_ = ms; }
    void setDepth(int d) { depth_ = d; }
    void setNodes(int n) { nodes_ = n; }
    void setInfinite(bool v = true) { infinite_ = v; }

    std::optional<int64_t> moveTimeMs() const { return movetimeMs_; }
    std::optional<int> depth() const { return depth_; }
    std::optional<int> nodes() const { return nodes_; }
    bool infinite() const { return infinite_.value_or(false); }

    GoLimits createGoLimits() const {
        GoLimits limits;

        limits.movetimeMs = movetimeMs_;
        limits.depth = depth_;
        limits.nodes = nodes_;
        limits.infinite = infinite_.value_or(false);

        return limits;
    }

private:
    std::optional<int64_t> movetimeMs_;
    std::optional<int> depth_;
    std::optional<int> nodes_;
    std::optional<bool> infinite_;
};

