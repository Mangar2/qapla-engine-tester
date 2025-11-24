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

#include "opening-parser.h"
#include "pgn-io.h"

#include <algorithm>
#include <ranges>
#include <cctype>

namespace QaplaTester {

OpeningParser::OpeningParser() {
    // Register PGN parser
    addParser("PGN", {".pgn"}, [](const std::filesystem::path& filePath) -> std::optional<std::vector<GameRecord>> {
        try {
            PgnIO pgnIO;
            auto games = pgnIO.loadGames(filePath.string());
            return games;
        } catch (...) {
            return std::nullopt;
        }
    });
}

void OpeningParser::addParser(const std::string& name, const std::vector<std::string>& extensions, const OpeningParserFunction& parser) {
    parsers_.push_back({name, extensions, parser});
}

std::vector<GameRecord> OpeningParser::parse(const std::filesystem::path& filePath) const {
    const auto extension = filePath.extension().string();
    
    // Helper to check if an extension matches (case-insensitive)
    auto matchesExtension = [&](const std::string& ext) {
        if (ext.length() != extension.length()) {
            return false;
        }
        return std::ranges::equal(ext, extension, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
    };

    // First try parsers with matching extension
    for (const auto& entry : parsers_) {
        for (const auto& ext : entry.extensions) {
            if (matchesExtension(ext)) {
                 if (auto result = entry.parser(filePath); result) {
                     return *result;
                 }
                 break; 
            }
        }
    }

    // Then try all other parsers that haven't been tried yet
    for (const auto& entry : parsers_) {
        bool alreadyTried = false;
        for (const auto& ext : entry.extensions) {
            if (matchesExtension(ext)) {
                alreadyTried = true;
                break;
            }
        }
        
        if (!alreadyTried) {
            if (auto result = entry.parser(filePath); result) {
                return *result;
            }
        }
    }

    return {};
}

} // namespace QaplaTester
