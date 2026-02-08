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
#include <iomanip>
#include <format>

namespace QaplaTester {

TimeSegment TimeSegment::fromString(std::string str) {
    TimeSegment segment;
    if (str.empty()) {
        return segment;
    }
    size_t slashPos = str.find('/');
    if (slashPos != std::string::npos) {
        auto moves = QaplaHelpers::to_int(str.substr(0, slashPos));
        if (moves) {
            segment.movesToPlay = *moves;
        }
        str = str.substr(slashPos + 1);
    }
    
    size_t plusPos = str.find('+');
    std::optional<double> baseTime;
    std::optional<double> increment;

    if (plusPos != std::string::npos) {
        baseTime = QaplaHelpers::parseDuration(str.substr(0, plusPos));
        increment = QaplaHelpers::parseDuration(str.substr(plusPos + 1));
    } else {
        baseTime = QaplaHelpers::parseDuration(str);
    }

    if (baseTime) {
        segment.baseTimeMs = static_cast<uint64_t>(*baseTime * 1000.0);
    }
    if (increment) {
        segment.incrementMs = static_cast<uint64_t>(*increment * 1000.0);
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
    if (!result.empty()) {
        return result;
    }
    if (infinite_.value_or(false)) {
        return "inf";
    }
    if (movetimeMs_) {
        return std::format("movetime(ms): {}", *movetimeMs_);
    }
    if (depth_) {
        return std::format("depth: {}", *depth_);
    }
    if (nodes_) {
        return std::format("nodes: {}", *nodes_);
    }
    if (mateIn_) {
        return std::format("mate: {}", *mateIn_);
    }
    return "";
}

TimeControl TimeControl::parse(const std::string& tc) {
    TimeControl timeControl;
    if (tc.empty()) {
        return timeControl;
    }
    timeControl.fromPgnTimeControlString(tc);
	return timeControl;
}

namespace {
    bool tryParseSpecial(const std::string& pgnString, TimeControl& tc) {
        using namespace QaplaHelpers;
        if (pgnString == "inf") {
            tc.setInfinite(true);
            return true;
        }
        if (pgnString.starts_with("movetime(ms):")) {
            auto value = to_unsigned_int<uint64_t>(pgnString.substr(13)); // length of "movetime(ms):" is 13
            if (value) {
                tc.setMoveTime(*value);
            }
            return true;
        }
        if (pgnString.starts_with("depth:")) {
            auto value = to_unsigned_int<uint32_t>(pgnString.substr(6));
            if (value) {
                tc.setDepth(*value);
            }
            return true;
        }
        if (pgnString.starts_with("nodes:")) {
            auto value = to_unsigned_int<uint64_t>(pgnString.substr(6));
            if (value) {
                tc.setNodes(*value);
            }
            return true;
        }
        if (pgnString.starts_with("mate:")) {
            auto value = to_unsigned_int<uint32_t>(pgnString.substr(5));
            if (value) {
                tc.setMateIn(*value);
            }
            return true;
        }
        return false;
    }
}

void TimeControl::fromPgnTimeControlString(const std::string& pgnString) {
    using namespace QaplaHelpers;
    
    timeSegments_.clear();
    infinite_.reset();
    movetimeMs_.reset();
    depth_.reset();
    nodes_.reset();
    mateIn_.reset();
    
    if (pgnString.empty()) {
        return;
    }
    
    if (tryParseSpecial(pgnString, *this)) {
        return;
    }

    // Try parsing as a single segment first to support "MM:SS" formats which contain colon
    // If we can successfully parse duration from the string (or parts), we prefer that over splitting by colon.
    bool parsedAsSingle = false;
    {
        // Check if parsing as single segment makes sense
        // Just try it.
        TimeSegment seg = TimeSegment::fromString(pgnString);
        // How to know if it succeeded fully?
        // We check if re-exporting it matches roughly? No.
        
        // Key marker: If the string has ONE colon and it's surrounded by digits, treat as single.
        // If it has multiple colons, or colon not surrounded by digits, treat as multi.
        // Or if it fits "moves/time+inc" pattern.
        
        // Let's count colons.
        size_t colons = std::ranges::count(pgnString, ':');
        bool likelyTime = false;
        if (colons == 1) {
             // check if it looks like time
             // simple regex-ish check
             size_t pos = pgnString.find(':');
             if (pos > 0 && pos + 1 < pgnString.size() && 
                 std::isdigit(static_cast<unsigned char>(pgnString[pos-1])) != 0 && 
                 std::isdigit(static_cast<unsigned char>(pgnString[pos+1])) != 0) {
                 likelyTime = true;
             }
        }
        else if (colons == 2) {
             // H:M:S ?
             // verify both colons surrounded
             likelyTime = true; // simplifying
        }
        
        if (likelyTime) {
            timeSegments_.push_back(seg);
            parsedAsSingle = true;
        }
    }
    
    if (!parsedAsSingle) {
        // Parse standard time control segments by splitting at ':'
        // NOTE: This will break if a segment contains ':' (e.g. MM:SS format) and we didn't catch it above.
        // This is the fallback for PGN compatibility.
        std::istringstream iss(pgnString);
        std::string segmentStr;
        while (std::getline(iss, segmentStr, ':')) {
             timeSegments_.push_back(TimeSegment::fromString(segmentStr));
        }
    }
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