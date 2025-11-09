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

#include "time-control.h"
#include "string-helper.h"

#include <optional>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <unordered_map>
#include <iomanip>

namespace QaplaTester {

TimeSegment TimeSegment::fromString(std::string str) {
    TimeSegment segment;
    if (str.empty()) {
        return segment;
    }
    size_t slashPos = str.find('/');
    if (slashPos != std::string::npos) {
        segment.movesToPlay = std::stoi(str.substr(0, slashPos));
        str = str.substr(slashPos + 1);
    }
    size_t plusPos = str.find('+');
    if (plusPos != std::string::npos) {
        segment.baseTimeMs = static_cast<uint64_t>(std::stod(str.substr(0, plusPos)) * 1000);
        segment.incrementMs = static_cast<uint64_t>(std::stod(str.substr(plusPos + 1)) * 1000);
    } else {
        segment.baseTimeMs = static_cast<uint64_t>(std::stod(str) * 1000);
    }
    return segment;
}

std::string to_string(TimeSegment segment, int basePrecision, int incrementPrecision) {
    std::ostringstream oss;
    if (segment.movesToPlay > 0) {
        oss << segment.movesToPlay << "/";
    }
    oss << std::fixed << std::setprecision(basePrecision)
        << static_cast<double>(segment.baseTimeMs) / 1000.0;
    if (segment.incrementMs > 0) {
        oss << "+" << std::fixed << std::setprecision(incrementPrecision)
            << static_cast<double>(segment.incrementMs) / 1000.0;
    }
	return oss.str();
}

std::string TimeControl::toPgnTimeControlString(int basePrecision, int incrementPrecision) const {
    std::string result;
    for (size_t i = 0; i < timeSegments_.size(); ++i) {
        const auto& segment = timeSegments_[i];
        if (i > 0) {
            result += ":";
        }
		result += to_string(segment, basePrecision, incrementPrecision);
    }
    return result;
}

TimeControl TimeControl::parse(const std::string& tc) {
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

void TimeControl::fromPgnTimeControlString(const std::string& pgnString) {
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
            segment.baseTimeMs = static_cast<uint64_t>(std::stod(segmentStr.substr(0, plusPos)) * 1000);
            segment.incrementMs = static_cast<uint64_t>(std::stod(segmentStr.substr(plusPos + 1)) * 1000);
        }
        else {
            segment.baseTimeMs = static_cast<uint64_t>(std::stod(segmentStr) * 1000);
        }
        timeSegments_.push_back(segment);
    }
}

void TimeControl::fromCliTimeControlString(const std::string& cliString) {
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
        segment.baseTimeMs = static_cast<uint64_t>(
            std::stof(timePart.substr(0, plusPos - (slashPos != std::string::npos ? slashPos + 1 : 0))) * 1000);
        segment.incrementMs = static_cast<uint64_t>(
            std::stof(timePart.substr(plusPos - (slashPos != std::string::npos ? slashPos + 1 : 0) + 1)) * 1000);
    }
    else {
        segment.baseTimeMs = static_cast<uint64_t>(std::stof(timePart) * 1000);
    }

    timeSegments_.push_back(segment);
    setInfinite(false);
}

QaplaHelpers::IniFile::Section TimeControl::toSection(const std::string& name) const {
    QaplaHelpers::IniFile::Section section;
    section.name = "timecontrol";
    section.addEntry("name", name);
    if (movetimeMs_) {
        section.addEntry("movetime", std::to_string(*movetimeMs_));
    }
    if (depth_) {
        section.addEntry("depth", std::to_string(*depth_));
    }
    if (nodes_) {
        section.addEntry("nodes", std::to_string(*nodes_));
    }
    if (mateIn_) {
        section.addEntry("matein", std::to_string(*mateIn_));
    }
    if (infinite_) {
        section.addEntry("infinite", (*infinite_ ? "true" : "false"));
    }
    if (!timeSegments_.empty()) {
        section.addEntry("tc", toPgnTimeControlString());
    }
    return section;
}

void TimeControl::fromSection(const QaplaHelpers::IniFile::Section& section) {
    for (const auto& [key, value] : section.entries) {
        if (key == "movetime") {
            movetimeMs_ = std::stoull(value);
        } else if (key == "depth") {
            depth_ = std::stoul(value);
        } else if (key == "nodes") {
            nodes_ = std::stoul(value);
        } else if (key == "matein") {
            mateIn_ = std::stoul(value);
        } else if (key == "infinite") {
            infinite_ = (value == "true");
        } else if (key == "tc") {
            fromPgnTimeControlString(value);
        }
    }
}

std::string to_string(const TimeControl& tc) {
	return tc.toPgnTimeControlString();
}

GoLimits createGoLimits(
    const TimeControl& white,
    const TimeControl& black,
    uint32_t halfMoves,
    uint64_t whiteTimeUsedMs,
    uint64_t blackTimeUsedMs,
    bool whiteToMove
) {
    if (!white.isValid() || !black.isValid()) {
        throw std::invalid_argument("Time control is not valid");
    }

    GoLimits limits;
    limits.moveTimeMs = white.moveTimeMs();
    limits.depth = white.depth();
    limits.nodes = white.nodes();
    limits.mateIn = white.mateIn();
    limits.infinite = white.infinite();

    if (limits.moveTimeMs || limits.depth || limits.nodes || limits.infinite) {
        limits.hasTimeControl = false;
        return limits;
    }

	limits.hasTimeControl = true;

    uint32_t wMovesPlayed = (halfMoves + 1) / 2;
    uint32_t bMovesPlayed = halfMoves / 2;

    auto compute = [](const TimeControl& tc, uint32_t movesPlayed,
        uint64_t timeUsedMs, uint64_t& timeLeftMs, uint64_t& incrementMs, uint32_t& movesToGo) {
        auto rem = static_cast<int32_t>(movesPlayed);
        size_t i = 0;
        timeLeftMs = 0;
        incrementMs = 0;
        movesToGo = 0;

        const auto& segments = tc.timeSegments();

        while (true) {
            const TimeSegment& seg = (i < segments.size()) ? segments[i] : segments.back();
            int32_t movesInSegment = seg.movesToPlay;

            if (movesInSegment == 0) {
                // Sudden death (no move count limit)
                timeLeftMs = seg.baseTimeMs + static_cast<uint64_t>(movesPlayed) * seg.incrementMs;
                incrementMs = seg.incrementMs;
                movesToGo = 0;
                break;
            }

            if (rem < movesInSegment) {
                timeLeftMs = seg.baseTimeMs + static_cast<uint64_t>(rem) * seg.incrementMs;
                incrementMs = seg.incrementMs;
                movesToGo = static_cast<uint32_t>(movesInSegment - rem);
                break;
            }

            // Fully consumed segment
            rem -= movesInSegment;
            ++i;
        }

        timeLeftMs = timeLeftMs < timeUsedMs ? 0 : timeLeftMs - timeUsedMs;
        };

    uint32_t wMovesToGo = 0;
    uint32_t bMovesToGo = 0;
    compute(white, wMovesPlayed, whiteTimeUsedMs, limits.wtimeMs, limits.wincMs, wMovesToGo);
    compute(black, bMovesPlayed, blackTimeUsedMs, limits.btimeMs, limits.bincMs, bMovesToGo);

    // Set correct movesToGo based on side to move
    limits.movesToGo = whiteToMove ? wMovesToGo : bMovesToGo;

    return limits;
}

} // namespace QaplaTester