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

#include <sstream>
#include <stdexcept>
#include <cctype>
#include <stdexcept>
#include <fstream>
#include <optional>
#include "epd-reader.h"

EpdReader::EpdReader(const std::string& filePath): filePath_(filePath) {
    std::ifstream file(filePath);
    if (!file) {
        throw std::runtime_error("Failed to open EPD file: " + filePath);
    }
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            entries_.emplace_back(parseEpdLine(line));
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
    std::string resultLine;
    std::getline(stream, resultLine);
    EpdEntry result;

    std::string rest;
    std::istringstream lineStream(resultLine);
    result.fen = extractFen(lineStream);
    std::getline(lineStream, rest);
    parseOperations(rest, result);

    return result;
}

std::string EpdReader::extractFen(std::istringstream& stream) {
    std::ostringstream fenStream;
    std::string token;
    for (int i = 0; i < 4; ++i) {
        if (!(stream >> token)) {
            throw std::runtime_error("Incomplete FEN in EPD line");
        }
        if (i > 0) fenStream << ' ';
        fenStream << token;
    }
    return fenStream.str();
}

void EpdReader::parseOperations(const std::string& input, EpdEntry& result) {
    std::istringstream stream(input);
    std::string token;
    std::string opCode;

    while (stream >> std::ws) {
        char ch = stream.peek();
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

