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

#include "../chess-game/game-record.h"
#include "fen-parser.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>

namespace QaplaTester {

/**
 * @brief Represents a single trace entry during EPD file processing.
 */
struct TraceEntry {
    enum class Level {
        Info,
        Warning,
        Error
    };

    Level level = Level::Info;
    std::string message;
    std::optional<size_t> lineNumber;  ///< Line number if applicable
    std::string context;                ///< Additional context (e.g., the actual line content)

    TraceEntry(Level lvl, std::string msg, std::optional<size_t> line = std::nullopt, std::string ctx = "")
        : level(lvl), message(std::move(msg)), lineNumber(line), context(std::move(ctx)) {}

    [[nodiscard]] std::string levelToString() const {
        switch (level) {
            case Level::Info: return "INFO";
            case Level::Warning: return "WARNING";
            case Level::Error: return "ERROR";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] std::string toString() const {
        std::string result = "[" + levelToString() + "] " + message;
        if (lineNumber.has_value()) {
            result += " (line " + std::to_string(lineNumber.value()) + ")";
        }
        if (!context.empty()) {
            result += ": " + context;
        }
        return result;
    }
};

/**
 * Represents the parsed contents of a single EPD line.
 */
struct EpdEntry {
    std::string fen; ///< Full FEN string (first 4 fields only)
    bool whiteToMove = true;   ///< Side to move, taken from the FEN
    uint32_t startHalfmoves = 0; ///< Halfmove number at the position, taken from the FEN
    std::unordered_map<std::string, std::vector<std::string>> operations; ///< Opcode to operands
};

/**
 * @brief Result of reading an EPD file with complete trace information.
 */
struct EpdReaderResult {
    std::vector<EpdEntry> entries;          ///< Successfully parsed entries
    size_t errorCount = 0;                  ///< Number of lines that failed to parse
    std::vector<TraceEntry> trace;          ///< Complete trace of the reading process
    std::string filePath;                   ///< Path of the processed file
    bool fileOpened = false;                ///< Whether the file was successfully opened
    std::chrono::milliseconds duration{0};  ///< Time taken to process the file
    
    /**
     * @brief Calculates the error rate.
     * @return Error rate as a value between 0.0 and 1.0.
     */
    [[nodiscard]] double getErrorRate() const {
        auto total = entries.size() + errorCount;
        return (total == 0) ? 0.0 : static_cast<double>(errorCount) / static_cast<double>(total);
    }
    
    /**
     * @brief Returns the total number of processed lines.
     * @return Total count of entries plus errors.
     */
    [[nodiscard]] size_t getTotalCount() const {
        return entries.size() + errorCount;
    }

    /**
     * @brief Returns only error-level trace entries.
     * @return Vector of error trace entries.
     */
    [[nodiscard]] std::vector<TraceEntry> getErrors() const {
        std::vector<TraceEntry> errors;
        for (const auto& entry : trace) {
            if (entry.level == TraceEntry::Level::Error) {
                errors.push_back(entry);
            }
        }
        return errors;
    }

    /**
     * @brief Returns the complete trace as a formatted string.
     * @return Formatted trace string.
     */
    [[nodiscard]] std::string getTraceString() const {
        std::string result;
        for (const auto& entry : trace) {
            result += entry.toString() + "\n";
        }
        return result;
    }
};

class EpdReader {
public:
    /**
     * @brief Constructs an EpdReader and loads all entries from the specified file.
     * @param filePath Path to the EPD file.
     * @param maxEntries Maximum number of entries to read (excluding empty lines). If nullopt, read all.
     * @param maxStoredErrorTraceEntries Maximum number of error trace entries to store (default 10).
     * @throws std::runtime_error if the file cannot be read.
     */
    explicit EpdReader(const std::string& filePath, 
                       std::optional<size_t> maxEntries = std::nullopt,
                       size_t maxStoredErrorTraceEntries = 10);

    /**
     * @brief Resets the reading position to the beginning.
     */
    void reset();

    /**
     * @brief Returns the next EPD entry if available.
     * @return std::optional containing the next EpdEntry, or std::nullopt if end reached.
     */
    std::optional<EpdEntry> next();

    /**
     * @brief Returns all loaded EPD entries.
     * @return const reference to the vector of all entries.
     */
    [[nodiscard]] const std::vector<EpdEntry>& all() const;

    /**
     * @brief Returns the file path of the EPD file.
     * @return const reference to the file path.
     */
	[[nodiscard]] const std::string& getFilePath() const {
		return filePath_;
	}

    /**
     * @brief Returns the number of lines that could not be parsed.
     * @return Number of parse errors.
     */
    [[nodiscard]] size_t getParseErrorCount() const {
        return parseErrorCount_;
    }

    /**
     * @brief Returns the trace entries collected during parsing.
     * @return const reference to the vector of trace entries.
     */
    [[nodiscard]] const std::vector<TraceEntry>& getTrace() const {
        return trace_;
    }

    /**
     * @brief Returns the complete result including entries, error count and error lines.
     * @return EpdReaderResult with all parsing information.
     */
    [[nodiscard]] EpdReaderResult getResult() const;

    /**
     * @brief Parses a FEN string starting from a given position.
     * @param input The FenParserInput structure containing the FEN string and parameters.
     * @return FenParserResult containing the parsed GameRecord and status.
     * @deprecated Thin wrapper around QaplaTester::parseFen (fen-parser.h); call that directly.
     */
    [[nodiscard]]
    static FenParserResult parseFen(const FenParserInput& input);

private:
    std::vector<EpdEntry> entries_;
    std::size_t currentIndex_;
    std::string filePath_;
    size_t parseErrorCount_ = 0;
    std::vector<TraceEntry> trace_;
    size_t maxStoredErrorTraceEntries_ = 10;
    size_t storedErrorTraceCount_ = 0;
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::milliseconds duration_{0};
    bool fileOpened_ = false;

    /**
     * @brief Parses a single EPD line into FEN and opcode-operand pairs.
     * @param line The EPD line as a string.
     * @return Parsed EpdEntry structure, fen is empty if parsing failed.
     */
    static EpdEntry parseEpdLine(const std::string& line);

    /**
     * @brief Parses a move list from the input string (e.g., "1. e4 e5 2. Nf3").
     * @param input The input string containing the move list.
     * @param startPos The position to start parsing from.
     * @param result The EpdEntry to store parsed moves (currently unused).
     * @return The position after the parsed move list.
     */
    static size_t parseMoveList(const std::string& input, size_t startPos, EpdEntry& result);

    /**
     * @brief Checks if an opcode looks like a move number (e.g., "1.", "23.").
     * @param opcode The opcode string to check.
     * @return true if the opcode matches the move number pattern.
     */
    [[nodiscard]]
    static bool isMoveNumber(const std::string& opcode);

    /**
     * @brief Parses operations (opcode-operand pairs) from the input string.
     * @param input The input string containing operations.
     * @param startPos The position to start parsing from.
     * @param result The EpdEntry to store parsed operations.
     */
    static void parseOperations(const std::string& input, size_t startPos, EpdEntry& result);

};

} // namespace QaplaTester
