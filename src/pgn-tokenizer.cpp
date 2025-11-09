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

#include "pgn-tokenizer.h"
#include <cctype>

namespace QaplaTester {

std::vector<std::string> PgnTokenizer::tokenize(const std::string& pgn) {
    std::vector<std::string> tokens;
    tokens.reserve(estimateTokenCount(pgn.size()));

    size_t pos = 0;
    const size_t length = pgn.size();

    while (pos < length) {
        skipWhitespace(pgn, pos);
        if (pos >= length) {
            break;
        }

        const char currentChar = pgn[pos];

        if (currentChar == '"') {
            tokens.emplace_back(extractQuotedString(pgn, pos));
        }
        else if (currentChar == ';') {
            tokens.emplace_back(extractLineComment(pgn, pos));
        }
        else if (isWordChar(currentChar)) {
            tokens.emplace_back(extractWord(pgn, pos));
        }
        else {
            // Default: all other characters as single-char tokens
            tokens.emplace_back(pgn, pos, 1);
            ++pos;
        }
    }

    return tokens;
}

size_t PgnTokenizer::estimateTokenCount(size_t pgnSize) {
    return pgnSize / 3 + 10;
}

void PgnTokenizer::skipWhitespace(const std::string& pgn, size_t& pos) {
    const size_t length = pgn.size();
    while (pos < length && std::isspace(static_cast<unsigned char>(pgn[pos])) != 0) {
        ++pos;
    }
}

std::string PgnTokenizer::extractQuotedString(const std::string& pgn, size_t& pos) {
    const size_t start = pos;
    ++pos;

    const size_t length = pgn.size();
    while (pos < length) {
        if (pgn[pos] == '\\' && pos + 1 < length) {
            pos += 2;
        }
        else if (pgn[pos] == '"') {
            ++pos;
            break;
        }
        else {
            ++pos;
        }
    }

    return { pgn, start, pos - start };
}

std::string PgnTokenizer::extractLineComment(const std::string& pgn, size_t& pos) {
    const size_t start = pos;
    const size_t length = pgn.size();

    while (pos < length && pgn[pos] != '\n' && pgn[pos] != '\r') {
        ++pos;
    }

    return { pgn, start, pos - start };
}

std::string PgnTokenizer::extractWord(const std::string& pgn, size_t& pos) {
    const size_t start = pos;
    const size_t length = pgn.size();

    // In PGN, start and continuation characters are identical, but for more general code
    // we still check explicitly: first character must be a valid word start
    if (pos < length && isWordStart(pgn[pos])) {
        ++pos;
        while (pos < length && isWordChar(pgn[pos])) {
            ++pos;
        }
    }

    return { pgn, start, pos - start };
}

bool PgnTokenizer::isWordStart(char c) {
    // "-" for negative numbers
    // "$" for annotation glyphs
    // "+" for positive numbers
    // "_" we allow words to start with underscore as well
    
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || 
           c == '-' || c == '$' || c == '+' || c == '_';
}

bool PgnTokenizer::isWordChar(char c) {
    // "-" for 1-0 results and castling notation
    // "." for decimal points in numeric annotations
    // "+" for check notation
    // "#" for checkmate notation
    // "=" for e8=Q promotions
    // "_" for NAGs like $6_1
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || 
           c == '-' || c == '.' || c == '+' || c == '#' || 
           c == '=' || c == '_';
}

} // namespace QaplaTester
