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

#include "epd-reader.h"
#include "string-helper.h"

#include <array>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <algorithm>

namespace QaplaTester {

EpdReader::EpdReader(const std::string& filePath): filePath_(filePath) {
    std::ifstream file(filePath);
    if (!file) {
        int err = errno;

        std::string errMsg;
    #ifdef _WIN32
        std::array<char, 256> buf{};
        strerror_s(buf.data(), buf.size(), err);
        errMsg = buf.data();
    #else
        std::error_code ec(err, std::generic_category());
        errMsg = ec.message();
    #endif
        throw std::runtime_error(
            "Failed to open EPD file: " + filePath +
            " (errno: " + std::to_string(err) + ", " + errMsg + ")"
        );
    }
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            ++lineNumber;
            auto entry = parseEpdLine(line);
            // Add auto-generated ID if not present
            if (!entry.operations.contains("id") || entry.operations["id"].empty()) {
                entry.operations["id"] = { std::to_string(lineNumber) };
            }
            entries_.emplace_back(std::move(entry));
        }
    }
    currentIndex_ = 0;
}

void EpdReader::reset() {
    currentIndex_ = 0;
}

std::optional<EpdEntry> EpdReader::next() {
    if (currentIndex_ < entries_.size()) {
        return entries_[currentIndex_++];
    }
    return std::nullopt;
}

const std::vector<EpdEntry>& EpdReader::all() const {
    return entries_;
}

EpdEntry EpdReader::parseEpdLine(const std::string& line) {
    std::istringstream stream(line);
    EpdEntry result;

    auto [fen, rest] = extractFen(stream); // liefert FEN + Rest
    result.fen = std::move(fen);
    parseOperations(rest, result);

    return result;
}

std::pair<std::string, std::string>
EpdReader::extractFen(std::istringstream& stream) {
    std::ostringstream fenStream;
    std::string token;

    for (int part = 0; part < 4; ++part) {
        if (!(stream >> token)) {
            throw std::runtime_error("Incomplete FEN in EPD line");
        }
        if (part != 0) {
            fenStream << ' ';
        }
        fenStream << token;
    }

    for (int i = 0; i < 2; ++i) {
        int v;
        bool isInt = static_cast<bool>(stream >> v);
        if (isInt && v >= 0) {
            fenStream << ' ' << v;
        } else {
            stream.clear();
            break;
        }
    }

    std::string rest;
    std::getline(stream, rest);
    return { fenStream.str(), rest };
}

void EpdReader::parseOperations(const std::string& input, EpdEntry& result) {
    // Remove all carriage returns from input before parsing
    std::string cleanedInput = input;
    std::erase(cleanedInput, '\r');
    
    std::istringstream stream(cleanedInput);
    std::string token;
    std::string opCode;

    while (stream >> std::ws) {
        char ch = static_cast<char>(stream.peek());
        if (ch == '"') {
            stream.get(); // skip opening quote
            std::getline(stream, token, '"');
        }
        else {
            stream >> token;
        }

        if (!token.empty() && token.back() == ';') {
            token.pop_back();
            if (!opCode.empty()) {
                result.operations[opCode].emplace_back(token);
                opCode.clear();
            }
            else {
                opCode = std::move(token);
                result.operations[opCode];
            }
        }
        else if (opCode.empty()) {
            opCode = std::move(token);
        }
        else {
            result.operations[opCode].emplace_back(token);
        }
    }
}

} // namespace QaplaTester
