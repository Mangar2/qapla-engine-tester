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

#include "string-helper.h"
#include "ini-file.h"

#include <optional>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <unordered_map>
#include <iomanip>

namespace QaplaTester {

/**
 * @brief Limits for calculating a single move.
 */
struct GoLimits {
    uint64_t wtimeMs = 0;
    uint64_t btimeMs = 0;
    uint64_t wincMs = 0;
    uint64_t bincMs = 0;
    uint32_t movesToGo = 0;
	bool hasTimeControl = false;  

    std::optional<uint32_t> depth;
    std::optional<uint64_t> nodes;
    std::optional<uint32_t> mateIn;
    std::optional<uint64_t> moveTimeMs;
    // List of moves to limit the search to (if supported by engine)
    std::optional<std::vector<std::string>> limitMoves;
    bool infinite = false;
};

/**
 * @brief Defines a stage in a time control setup.
 */
struct TimeSegment {
    bool operator==(const TimeSegment& other) const = default;
    int movesToPlay = 0;            ///< Number of moves in this time segment (0 = sudden death)
    uint64_t baseTimeMs = 0;         ///< Time for this segment in milliseconds
    uint64_t incrementMs = 0;        ///< Increment per move in milliseconds
    /**
     * @brief Creates a TimeSegment from a string representation.
     * @param str The string to parse (e.g., "40/300+2").
     * @return The parsed TimeSegment.
     */
    static TimeSegment fromString(std::string str);
};

/**
 * @brief Converts a TimeSegment to its string representation.
 * @param segment The TimeSegment to convert.
 * @param basePrecision Precision for base time.
 * @param incrementPrecision Precision for increment.
 * @return The string representation.
 */
std::string to_string(TimeSegment segment, int basePrecision = 1, int incrementPrecision = 2);

/**
 * @brief User-facing representation of a test time control.
 */
class TimeControl {
public:
    bool operator==(const TimeControl& other) const = default;

    /**
     * @brief Checks if the time control is valid.
     * @return True if valid, false otherwise.
     */
    [[nodiscard]] bool isValid() const {
		return !timeSegments_.empty() || infinite_.value_or(false) ||
			movetimeMs_.has_value() || depth_.has_value() || nodes_.has_value() || mateIn_.has_value();
    }

    /**
     * @brief Sets the move time in milliseconds.
     * @param ms The move time.
     */
    void setMoveTime(uint64_t ms) { movetimeMs_ = ms; }
    /**
     * @brief Sets the search depth.
     * @param d The depth.
     */
    void setDepth(uint32_t d) { depth_ = d; }
    /**
     * @brief Sets the node limit.
     * @param n The number of nodes.
     */
    void setNodes(uint32_t n) { nodes_ = n; }
    /**
     * @brief Sets the infinite search flag.
     * @param v The flag value.
     */
    void setInfinite(bool v = true) { infinite_ = v; }
	/**
     * @brief Sets the mate in moves.
     * @param m The number of moves to mate.
     */
	void setMateIn(uint32_t m) { mateIn_ = m; }

    /**
     * @brief Adds a time segment to the time control.
     * @param segment The time segment to add.
     */
    void addTimeSegment(const TimeSegment& segment) {
        timeSegments_.push_back(segment);
    }

    /**
     * @brief Gets the move time in milliseconds.
     * @return The move time.
     */
    [[nodiscard]] std::optional<uint64_t> moveTimeMs() const { return movetimeMs_; }
    /**
     * @brief Gets the search depth.
     * @return The depth.
     */
    [[nodiscard]] std::optional<uint32_t> depth() const { return depth_; }
    /**
     * @brief Gets the node limit.
     * @return The number of nodes.
     */
    [[nodiscard]] std::optional<uint32_t> nodes() const { return nodes_; }
	/**
     * @brief Gets the mate in moves.
     * @return The number of moves to mate.
     */
	[[nodiscard]] std::optional<uint32_t> mateIn() const { return mateIn_; }
    /**
     * @brief Checks if infinite search is enabled.
     * @return True if infinite, false otherwise.
     */
    [[nodiscard]] bool infinite() const { return infinite_.value_or(false); }
	/**
     * @brief Gets the time segments.
     * @return The vector of time segments.
     */
	[[nodiscard]] std::vector<TimeSegment> timeSegments() const { return timeSegments_; }

    /**
     * @brief Converts the time control to PGN time control string.
     * @param basePrecision Precision for base time.
     * @param incrementPrecision Precision for increment.
     * @return The PGN string.
     */
    [[nodiscard]] std::string toPgnTimeControlString(int basePrecision = 1, int incrementPrecision = 2) const;

    /**
     * @brief Parses a time control from a string.
     * @param tc The string to parse.
     * @return The parsed TimeControl.
     */
    static TimeControl parse(const std::string& tc);

    /**
     * @brief Parses a PGN time control string and sets up the time control.
     * @param pgnString The PGN string to parse.
     */
    void fromPgnTimeControlString(const std::string& pgnString);

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
    void fromCliTimeControlString(const std::string& cliString);

    /**
     * @brief Converts the time control to an INI section.
     * @param name The name of the section.
     * @return The INI section.
     */
    [[nodiscard]] QaplaHelpers::IniFile::Section toSection(const std::string& name) const;

    /**
     * @brief Loads the time control from an INI section.
     * @param section The INI section.
     */
    void fromSection(const QaplaHelpers::IniFile::Section& section);


private:
    std::optional<uint64_t> movetimeMs_;
    std::optional<uint32_t> depth_;
    std::optional<uint32_t> nodes_;
    std::optional<uint32_t> mateIn_;
    std::optional<bool> infinite_;
    std::vector<TimeSegment> timeSegments_;
};

/**
 * @brief Converts a TimeControl to its string representation.
 * @param tc The TimeControl to convert.
 * @return The string representation.
 */
std::string to_string(const TimeControl& tc);

/**
 * @brief Creates GoLimits from two time control definitions.
 *
 * This function evaluates time usage and time control structure separately for white and black.
 * It then computes the respective remaining time, increment, and movesToGo. The result is
 * populated into a GoLimits struct for UCI communication.
 *
 * @param white TimeControl settings for the white side.
 * @param black TimeControl settings for the black side.
 * @param halfMoves Number of half-moves already played in the game.
 * @param whiteTimeUsedMs Time used by white so far, in milliseconds.
 * @param blackTimeUsedMs Time used by black so far, in milliseconds.
 * @param whiteToMove Whether white is to move next.
 * @return GoLimits containing time data for both sides.
 */
GoLimits createGoLimits(
    const TimeControl& white,
    const TimeControl& black,
    uint32_t halfMoves,
    uint64_t whiteTimeUsedMs,
    uint64_t blackTimeUsedMs,
    bool whiteToMove
);

} // namespace QaplaTester
