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
#include <vector>
#include <optional>
#include <cstdint>

#include "engine-adapter.h"


 /**
  * @brief Defines a stage in a time control setup.
  */
struct TimeSegment {
    int movesToPlay = 0;            ///< Number of moves in this time segment (0 = sudden death)
    int64_t baseTimeMs = 0;         ///< Time for this segment in milliseconds
    int64_t incrementMs = 0;        ///< Increment per move in milliseconds
};

/**
 * @brief User-facing representation of a test time control.
 */
class TimeControl {
public:
    void setMoveTime(int64_t ms) { movetimeMs_ = ms; }
    void setDepth(int d) { depth_ = d; }
    void setNodes(int n) { nodes_ = n; }
    void setInfinite(bool v = true) { infinite_ = v; }
	void setMateIn(int m) { mateIn_ = m; }

    void addTimeSegment(const TimeSegment& segment) {
        timeSegments_.push_back(segment);
    }

    std::optional<int64_t> moveTimeMs() const { return movetimeMs_; }
    std::optional<int> depth() const { return depth_; }
    std::optional<int> nodes() const { return nodes_; }
	std::optional<int> mateIn() const { return mateIn_; }
    bool infinite() const { return infinite_.value_or(false); }

    /**
     * @brief Creates a GoLimits instance based on current time control settings.
     *
     * @param playedMoves Number of full moves already played.
     * @param whiteClockMs White player's remaining time in milliseconds.
     * @param blackClockMs Black player's remaining time in milliseconds.
     * @return GoLimits filled according to the currently active time control.
     */
    GoLimits createGoLimits(int playedMoves = 0, int64_t whiteClockMs = 0, int64_t blackClockMs = 0) const {
        GoLimits limits;

        if (movetimeMs_) {
            limits.movetimeMs = movetimeMs_;
        }

        limits.depth = depth_;
        limits.nodes = nodes_;
		limits.mateIn = mateIn_;
        limits.infinite = infinite_.value_or(false);

        // Later expansion: derive correct segment from playedMoves
        if (!timeSegments_.empty() && !movetimeMs_) {
            const TimeSegment& seg = timeSegments_.front();  // Currently defaulting to first

            limits.wtimeMs = seg.baseTimeMs;
            limits.btimeMs = seg.baseTimeMs;
            limits.wincMs = seg.incrementMs;
            limits.bincMs = seg.incrementMs;
            limits.movesToGo = seg.movesToPlay;
        }

        return limits;
    }

private:
    std::optional<int64_t> movetimeMs_;
    std::optional<int> depth_;
    std::optional<int> nodes_;
    std::optional<int> mateIn_;
    std::optional<bool> infinite_;
    std::vector<TimeSegment> timeSegments_;
};
