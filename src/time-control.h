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
#include <string>
#include <sstream>
#include <iomanip>


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
    bool operator==(const TimeSegment& other) const = default;
    int movesToPlay = 0;            ///< Number of moves in this time segment (0 = sudden death)
    int64_t baseTimeMs = 0;         ///< Time for this segment in milliseconds
    int64_t incrementMs = 0;        ///< Increment per move in milliseconds
};

/**
 * @brief User-facing representation of a test time control.
 */
class TimeControl {
public:
    bool operator==(const TimeControl& other) const = default;

    bool isValid() const {
		return !timeSegments_.empty() || infinite_.value_or(false) ||
			movetimeMs_.has_value() || depth_.has_value() || nodes_.has_value() || mateIn_.has_value();
    }

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
	std::vector<TimeSegment> timeSegments() const { return timeSegments_; }

    std::string toPgnTimeControlString() const {
        std::ostringstream oss;
        for (size_t i = 0; i < timeSegments_.size(); ++i) {
            const auto& segment = timeSegments_[i];
            if (i > 0) {
                oss << ":";
            }
            if (segment.movesToPlay > 0) {
                oss << segment.movesToPlay << "/";
            }
            oss << std::fixed << std::setprecision(1) << segment.baseTimeMs / 1000.0;
            if (segment.incrementMs > 0) {
                oss << "+" << std::fixed << std::setprecision(2) << segment.incrementMs / 1000.0;
            }
        }
        return oss.str();
    }

    static TimeControl parse(const std::string tc) {
        TimeControl timeControl;
        if (tc.empty()) {
            return timeControl;
        }
        if (tc == "inf") {
            timeControl.setInfinite(true);
            return timeControl;
        }
        timeControl.fromPgnTimeControlString(tc);
		return timeControl;
    }

    void fromPgnTimeControlString(const std::string& pgnString) {
        timeSegments_.clear();
        std::istringstream iss(pgnString);
        std::string segmentStr;
        while (std::getline(iss, segmentStr, ':')) {
            TimeSegment segment;
            size_t slashPos = segmentStr.find('/');
            if (slashPos != std::string::npos) {
                segment.movesToPlay = std::stoi(segmentStr.substr(0, slashPos));
                segmentStr = segmentStr.substr(slashPos + 1);
            }
            size_t plusPos = segmentStr.find('+');
            if (plusPos != std::string::npos) {
                segment.baseTimeMs = static_cast<int64_t>(std::stod(segmentStr.substr(0, plusPos)) * 1000);
                segment.incrementMs = static_cast<int64_t>(std::stod(segmentStr.substr(plusPos + 1)) * 1000);
            }
            else {
                segment.baseTimeMs = static_cast<int64_t>(std::stod(segmentStr) * 1000);
            }
            timeSegments_.push_back(segment);
        }
    }


    /**
     * @brief Parses a cutechess-cli-style time control string and sets up the time control.
     *
     * Supported formats:
     * - "moves/time+increment", e.g. "40/300+2" (40 moves in 300 seconds with 2s increment)
     * - "time+increment", e.g. "300+2" (sudden death with 2s increment)
     * - "time", e.g. "60" (sudden death, no increment)
     * - "inf" for infinite time control
     *
     * Time and increment are interpreted as seconds and can include decimals (e.g. "0.5").
     *
     * @param cliString The time control string to parse.
     */
    void fromCliTimeControlString(const std::string& cliString) {
        timeSegments_.clear();
		if (cliString.empty()) {
			return;
		}

        if (cliString == "inf") {
            setInfinite(true);
            return;
        }

        TimeSegment segment;

        size_t slashPos = cliString.find('/');
        size_t plusPos = cliString.find('+');

        if (slashPos != std::string::npos) {
            segment.movesToPlay = std::stoi(cliString.substr(0, slashPos));
        }

        std::string timePart = (slashPos != std::string::npos) ? cliString.substr(slashPos + 1) : cliString;

        if (plusPos != std::string::npos) {
            segment.baseTimeMs = static_cast<int64_t>(std::stof(timePart.substr(0, plusPos - (slashPos != std::string::npos ? slashPos + 1 : 0))) * 1000);
            segment.incrementMs = static_cast<int64_t>(std::stof(timePart.substr(plusPos - (slashPos != std::string::npos ? slashPos + 1 : 0) + 1)) * 1000);
        }
        else {
            segment.baseTimeMs = static_cast<int64_t>(std::stof(timePart) * 1000);
        }

        timeSegments_.push_back(segment);
        setInfinite(false);
    }


private:
    std::optional<int64_t> movetimeMs_;
    std::optional<int> depth_;
    std::optional<int> nodes_;
    std::optional<int> mateIn_;
    std::optional<bool> infinite_;
    std::vector<TimeSegment> timeSegments_;
};

inline std::string to_string(const TimeControl& tc) {
	return tc.toPgnTimeControlString();
}

/**
 * @brief Creates GoLimits from two time control definitions.
 *
 * This function evaluates time usage and time control structure separately for white and black.
 * It then computes the respective remaining time, increment, and movesToGo. The result is
 * populated into a GoLimits struct for UCI communication.
 *
 * @param white TimeControl settings for the white side.
 * @param black TimeControl settings for the black side.
 * @param playedMoves Number of full moves already played in the game.
 * @param whiteTimeUsedMs Time used by white so far, in milliseconds.
 * @param blackTimeUsedMs Time used by black so far, in milliseconds.
 * @param whiteToMove Whether white is to move next.
 * @return GoLimits containing time data for both sides.
 */
inline GoLimits createGoLimits(
    const TimeControl& white,
    const TimeControl& black,
    int playedMoves,
    int64_t whiteTimeUsedMs,
    int64_t blackTimeUsedMs,
    bool whiteToMove
) {
	if (!white.isValid()) {
		throw std::invalid_argument("White time control is not valid");
	}
	if (!black.isValid()) {
		throw std::invalid_argument("Black time control is not valid");
	}
	if (playedMoves < 0) {
		throw std::invalid_argument("Played moves cannot be negative");
	}
	if (whiteTimeUsedMs < 0 || blackTimeUsedMs < 0) {
		throw std::invalid_argument("Time used cannot be negative");
	}
    GoLimits limits;

    limits.movetimeMs = white.moveTimeMs();
    limits.depth = white.depth();
    limits.nodes = white.nodes();
    limits.mateIn = white.mateIn();
    limits.infinite = white.infinite();

    if (limits.movetimeMs || limits.depth || limits.nodes || limits.infinite) {
        return limits;
    }

    int64_t wTotal = 0, bTotal = 0;
    int64_t wInc = 0, bInc = 0;
    int wMovesToGo = 0, bMovesToGo = 0;

    auto compute = [&](const TimeControl& tc, int64_t timeUsedMs, int64_t& totalTime, int64_t& increment, int& movesToGo) {
        int rem = playedMoves;
        size_t i = 0;
        while (true) {
            const auto& segments = tc.timeSegments();
            const TimeSegment& seg = (i < segments.size()) ? segments[i] : segments.back();
            int moves = seg.movesToPlay;
            int segMoves = moves ? std::min(rem, moves) : rem;
            totalTime += seg.baseTimeMs + int64_t(segMoves) * seg.incrementMs;
            increment = seg.incrementMs;

            if (!seg.movesToPlay) {
                movesToGo = 0;
                break;
            }

            if (rem < moves) {
                movesToGo = moves - rem;
                break;
            }

            rem -= moves;
            ++i;
        }

        totalTime = totalTime > timeUsedMs ? totalTime - timeUsedMs : 0;
        };

    compute(white, whiteTimeUsedMs, limits.wtimeMs, limits.wincMs, wMovesToGo);
    compute(black, blackTimeUsedMs, limits.btimeMs, limits.bincMs, bMovesToGo);
    limits.movesToGo = whiteToMove ? wMovesToGo : bMovesToGo;

    return limits;
}
