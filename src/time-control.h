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


 /**
  * @brief limits for calculating a single move.
  */
struct GoLimits {
    int64_t wtimeMs = 0;
    int64_t btimeMs = 0;
    int64_t wincMs = 0;
    int64_t bincMs = 0;
    int32_t movesToGo = 0;

    std::optional<int> depth;
    std::optional<int> nodes;
    std::optional<int> mateIn;
    std::optional<int64_t> movetimeMs;
    std::optional<std::vector<std::string>> limitMoves;
    bool infinite = false;
};

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
     * @param whiteTimeUsedMs Time consumed by white so far, in milliseconds.
     * @param blackTimeUsedMs Time consumed by black so far, in milliseconds.
     * @return GoLimits filled according to the currently active time control.
     */
    GoLimits createGoLimits(int playedMoves = 0, int64_t whiteTimeUsedMs = 0, int64_t blackTimeUsedMs = 0) const {
        GoLimits limits;

        if (movetimeMs_) {
            limits.movetimeMs = movetimeMs_;
        }

        limits.depth = depth_;
        limits.nodes = nodes_;
        limits.mateIn = mateIn_;
        limits.infinite = infinite_.value_or(false);

        if (timeSegments_.empty() || movetimeMs_) {
            return limits;
        }

        int remainingMoves = playedMoves;
        int64_t whiteTotalTime = 0;
        int64_t blackTotalTime = 0;
        int64_t whiteIncrement = 0;
        int64_t blackIncrement = 0;
        int nextMovesToGo = 0;

        size_t i = 0;
        while (true) {
            const TimeSegment& seg = (i < timeSegments_.size()) ? timeSegments_[i] : timeSegments_.back();

            int segmentMoves = seg.movesToPlay;
            int movesInThisSegment = segmentMoves ? std::min(remainingMoves, segmentMoves): remainingMoves;
            whiteTotalTime += seg.baseTimeMs + static_cast<int64_t>(movesInThisSegment) * seg.incrementMs;
            blackTotalTime += seg.baseTimeMs + static_cast<int64_t>(movesInThisSegment) * seg.incrementMs;
            whiteIncrement = seg.incrementMs;
            blackIncrement = seg.incrementMs;

            if (!seg.movesToPlay) {
                nextMovesToGo = 0;
                break;
            }

            if (remainingMoves < segmentMoves) {
                nextMovesToGo = segmentMoves - remainingMoves;
                break;
            }

            remainingMoves -= segmentMoves;
            ++i;
        }

        limits.wtimeMs = whiteTotalTime > whiteTimeUsedMs ? whiteTotalTime - whiteTimeUsedMs : 0;
        limits.btimeMs = blackTotalTime > blackTimeUsedMs ? blackTotalTime - blackTimeUsedMs : 0;
        limits.wincMs = whiteIncrement;
        limits.bincMs = blackIncrement;
        limits.movesToGo = nextMovesToGo;

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
