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

#include "game-record.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief Trace information for a single parser attempt.
 */
struct ParserTraceEntry {
    std::string parserName;             ///< Name of the parser that was tried
    bool success = false;               ///< Whether the parser succeeded
    std::vector<std::string> messages;  ///< Trace messages from this parser
};

/**
 * @brief Parameters for parsing opening files.
 */
struct OpeningParserParams {
    std::optional<size_t> maxGames;     ///< Maximum number of games to load (nullopt = all)
};

/**
 * @brief Result of parsing an opening file.
 */
struct OpeningParserResult {
    std::vector<GameRecord> games;              ///< Parsed game records
    std::string filePath;                       ///< Path to the parsed file
    std::string successfulParser;               ///< Name of the parser that succeeded (empty if none)
    std::vector<ParserTraceEntry> trace;        ///< Trace information from all attempted parsers
    
    /**
     * @brief Returns whether parsing was successful.
     * @return true if at least one game was parsed.
     */
    [[nodiscard]] bool success() const {
        return !games.empty();
    }
};

/**
 * @brief Function type for parsing opening files.
 * 
 * Takes a file path, a trace entry reference to fill with diagnostic info,
 * and optional parameters for parsing.
 * Returns a vector of GameRecords if successful, or std::nullopt if the parser 
 * cannot handle the file or fails.
 */
using OpeningParserFunction = std::function<std::optional<std::vector<GameRecord>>(
    const std::filesystem::path&, ParserTraceEntry&, const OpeningParserParams&)>;

/**
 * @brief Manages parsing of opening files in various formats.
 * 
 * Allows registration of parsers for different file formats.
 * Tries to parse files using registered parsers, prioritizing those
 * matching the file extension.
 */
class OpeningParser {
public:
    /**
     * @brief Constructs the OpeningParser and registers default parsers.
     */
    OpeningParser();

    /**
     * @brief Registers a new parser.
     * 
     * @param name The name of the parser.
     * @param extensions List of file extensions this parser supports (e.g., ".pgn").
     * @param parser The parser function.
     */
    void addParser(const std::string& name, const std::vector<std::string>& extensions, const OpeningParserFunction& parser);

    /**
     * @brief Parses a file and returns the game records.
     * 
     * Tries parsers matching the file extension first, then other parsers.
     * 
     * @param filePath The path to the file to parse.
     * @param maxGames Maximum number of games to load (nullopt = all).
     * @return A vector of GameRecords found in the file. Returns empty vector if no games found or parsing failed.
     */
    [[nodiscard]] std::vector<GameRecord> parse(const std::filesystem::path& filePath, 
                                                 std::optional<size_t> maxGames = std::nullopt) const;

    /**
     * @brief Parses a file and returns detailed result with trace information.
     * 
     * Tries parsers matching the file extension first, then other parsers.
     * 
     * @param filePath The path to the file to parse.
     * @param maxGames Maximum number of games to load (nullopt = all).
     * @return OpeningParserResult containing games, parser info, and trace.
     */
    [[nodiscard]] OpeningParserResult parseWithTrace(const std::filesystem::path& filePath,
                                                      std::optional<size_t> maxGames = std::nullopt) const;

private:
    struct ParserEntry {
        std::string name;
        std::vector<std::string> extensions;
        OpeningParserFunction parser;
    };

    std::vector<ParserEntry> parsers_;
};

} // namespace QaplaTester
