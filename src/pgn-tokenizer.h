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

#include <string>
#include <vector>
#include <string_view>

namespace QaplaTester {

/**
 * @class PgnTokenizer
 * @brief High-performance tokenizer for Portable Game Notation (PGN) strings
 * 
 * Splits PGN input into individual tokens with optimal performance:
 * - Pre-allocates vector capacity to minimize reallocations
 * - Uses single-copy construction via emplace_back
 * - Extracts tokens directly from source string using substring operations
 * - Avoids character-by-character string building
 * 
 * Recognized token types:
 * - Tag pairs: [TagName "value"]
 * - Move numbers: 1. 2. 3. etc.
 * - Moves: e4, Nf3, O-O, etc.
 * - Annotations: $1, $2, etc.
 * - Comments: { and } are separate tokens, content is tokenized
 * - Line comments: ; comment (as single token including semicolon)
 * - Variations: (1... e5 2. Nf3) - parentheses are separate tokens
 * - Results: 1-0, 0-1, 1/2-1/2, *
 * - Punctuation: [ ] ( ) . { } ;
 * - Quoted strings: "string content" (as single token including quotes)
 */
class PgnTokenizer {
public:
    /**
     * @brief Tokenizes a PGN string into a vector of token strings
     * @param pgn The PGN string to tokenize
     * @return Vector of tokens extracted from the input
     */
    static std::vector<std::string> tokenize(const std::string& pgn);

private:
    /**
     * @brief Estimates initial vector capacity based on input size
     * @param pgnSize Size of the input PGN string
     * @return Estimated number of tokens (heuristic: ~10 chars per token)
     */
    static size_t estimateTokenCount(size_t pgnSize);

    /**
     * @brief Skips whitespace characters at current position
     * @param pgn Source PGN string
     * @param pos Current position (will be updated)
     */
    static void skipWhitespace(const std::string& pgn, size_t& pos);

    /**
     * @brief Extracts a quoted string token including quotes
     * @param pgn Source PGN string
     * @param pos Current position (will be updated to position after closing quote)
     * @return Token string including opening and closing quotes
     */
    static std::string extractQuotedString(const std::string& pgn, size_t& pos);

    /**
     * @brief Extracts a line comment token from semicolon to end of line
     * @param pgn Source PGN string
     * @param pos Current position (will be updated to position after newline)
     * @return Token string including semicolon and comment text
     */
    static std::string extractLineComment(const std::string& pgn, size_t& pos);

    /**
     * @brief Extracts a word/symbol token (moves, tags, numbers, NAG, results)
     * @param pgn Source PGN string
     * @param pos Current position (will be updated to position after token)
     * @return Token string
     */
    static std::string extractWord(const std::string& pgn, size_t& pos);

    /**
     * @brief Checks if character is valid as word start character
     * @param c Character to check
     * @return true if c can start a word token
     * @note In PGN this is identical to isWordChar, but kept separate for clarity
     */
    static bool isWordStart(char c);

    /**
     * @brief Checks if character is valid within a word token
     * @param c Character to check
     * @return true if c is alphanumeric, -, /, $, +, #, =, or _
     */
    static bool isWordChar(char c);
};

} // namespace QaplaTester
