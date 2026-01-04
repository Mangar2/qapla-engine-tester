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
#include "game-state.h"

#include "qapla-engine/fenscanner.h"
#include "qapla-engine/movegenerator.h"

#include <array>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <algorithm>

namespace QaplaTester {

EpdReader::EpdReader(const std::string& filePath, 
                     std::optional<size_t> maxEntries,
                     size_t maxStoredErrorTraceEntries)
    : filePath_(filePath), maxStoredErrorTraceEntries_(maxStoredErrorTraceEntries) {
    
    startTime_ = std::chrono::steady_clock::now();
    trace_.emplace_back(TraceEntry::Level::Info, "Starting EPD file processing", std::nullopt, filePath);
    
    std::ifstream file(filePath);
    if (!file) {
        fileOpened_ = false;
        auto err = errno;

        std::string errMsg;
    #ifdef _WIN32
        std::array<char, 256> buf{};
        strerror_s(buf.data(), buf.size(), err);
        errMsg = buf.data();
    #else
        std::error_code errCode(err, std::generic_category());
        errMsg = errCode.message();
    #endif
        
        trace_.emplace_back(TraceEntry::Level::Error, 
            "Failed to open file (errno: " + std::to_string(err) + ", " + errMsg + ")",
            std::nullopt, filePath);
        
        duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_);
        trace_.emplace_back(TraceEntry::Level::Info, 
            "Processing finished with errors after " + std::to_string(duration_.count()) + "ms");
        
        throw std::runtime_error(
            "Failed to open EPD file: " + filePath +
            " (errno: " + std::to_string(err) + ", " + errMsg + ")"
        );
    }
    
    fileOpened_ = true;
    trace_.emplace_back(TraceEntry::Level::Info, "File opened successfully");
    
    std::string line;
    size_t lineNumber = 0;
    size_t processedCount = 0;
    size_t emptyLineCount = 0;
    
    while (std::getline(file, line)) {
        ++lineNumber;
        if (line.empty()) {
            ++emptyLineCount;
            continue;
        }
        ++processedCount;
        if (maxEntries.has_value() && processedCount > maxEntries.value()) {
            trace_.emplace_back(TraceEntry::Level::Info, 
                "Reached maximum entry limit", lineNumber, 
                "max=" + std::to_string(maxEntries.value()));
            break;
        }
        auto entry = parseEpdLine(line);
        if (entry.fen.empty()) {
            ++parseErrorCount_;
            if (storedErrorTraceCount_ < maxStoredErrorTraceEntries_) {
                trace_.emplace_back(TraceEntry::Level::Error, 
                    "Failed to parse EPD line", lineNumber, 
                    line.length() > 100 ? line.substr(0, 100) + "..." : line);
                ++storedErrorTraceCount_;
            }
            continue;
        }
        // Add auto-generated ID if not present
        if (!entry.operations.contains("id") || entry.operations["id"].empty()) {
            entry.operations["id"] = { std::to_string(lineNumber) };
        }
        entries_.emplace_back(std::move(entry));
    }
    
    // Check for file read errors
    if (file.bad()) {
        trace_.emplace_back(TraceEntry::Level::Error, 
            "File read error occurred during processing", lineNumber);
    }
    
    currentIndex_ = 0;
    
    duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime_);
    
    // Add summary trace entries
    trace_.emplace_back(TraceEntry::Level::Info, 
        "Total lines read: " + std::to_string(lineNumber));
    if (emptyLineCount > 0) {
        trace_.emplace_back(TraceEntry::Level::Info, 
            "Empty lines skipped: " + std::to_string(emptyLineCount));
    }
    trace_.emplace_back(TraceEntry::Level::Info, 
        "Successfully parsed entries: " + std::to_string(entries_.size()));
    if (parseErrorCount_ > 0) {
        trace_.emplace_back(TraceEntry::Level::Warning, 
            "Parse errors: " + std::to_string(parseErrorCount_) + 
            " (" + std::to_string(storedErrorTraceCount_) + " traced)");
    }
    trace_.emplace_back(TraceEntry::Level::Info, 
        "Processing completed in " + std::to_string(duration_.count()) + "ms");
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

EpdReaderResult EpdReader::getResult() const {
    EpdReaderResult result;
    result.entries = entries_;
    result.errorCount = parseErrorCount_;
    result.trace = trace_;
    result.filePath = filePath_;
    result.fileOpened = fileOpened_;
    result.duration = duration_;
    return result;
}

EpdEntry EpdReader::parseEpdLine(const std::string& line) {
    EpdEntry result;

    FenParserInput input;
    input.fenString = line;
    input.startPos = 0;
    input.maxSearchLength = 10;

    auto fenResult = parseFen(input);
    if (!fenResult.gameRecord) {
        return result;
    }

    result.fen = fenResult.gameRecord->getStartFen();
    
    // Check for move list before operations
    auto posAfterMoves = parseMoveList(line, fenResult.nextPos, result);
    parseOperations(line, posAfterMoves, result);

    return result;
}

FenParserResult EpdReader::parseFen(const FenParserInput& input) {
    FenParserResult result;
    result.nextPos = input.startPos;

    // skip blanks at the beginning
    size_t startPos = input.fenString.find_first_not_of(' ', input.startPos);
    if (startPos == std::string::npos) {
        return result;
    }

    size_t searchLength = std::min(input.fenString.length(), input.maxSearchLength + startPos);
    
    for (; startPos < searchLength; ++startPos) {
        
        QaplaInterface::FenScanner scanner;
        QaplaMoveGenerator::MoveGenerator position;
        try {
            result.nextPos = scanner.setBoard(input.fenString, position, startPos);
            if (result.nextPos == 0) {
                continue;
            }
            std::string fen = input.fenString.substr(startPos, result.nextPos - startPos);
            GameState gameState;
            gameState.setFen(false, fen);
            
            result.gameRecord = GameRecord();
            result.gameRecord->setStartPosition(
                false,                              // Not standard start position
                fen,                                // FEN string
                gameState.isWhiteToMove(),          // Who to move
                gameState.getStartHalfmoves()       // Half-move clock
            );
            return result;
        } catch (...) {
            // Continue searching if GameState creation fails
            continue;
        }
    }
    if (!result.gameRecord) {
        result.error = 1; 
    }

    return result;
}

bool EpdReader::isMoveNumber(const std::string& opcode) {
    if (opcode.empty() || opcode.back() != '.') {
        return false;
    }
    // Check if all characters before the dot are digits
    for (size_t idx = 0; idx < opcode.size() - 1; ++idx) {
        if (std::isdigit(static_cast<unsigned char>(opcode[idx])) == 0) {
            return false;
        }
    }
    return opcode.size() > 1;
}

size_t EpdReader::parseMoveList(const std::string& /*input*/, size_t startPos, EpdEntry& /*result*/) {
    // TODO: Implement move list parsing (e.g., "1. e4 e5 2. Nf3")
    return startPos;
}

void EpdReader::parseOperations(const std::string& input, size_t startPos, EpdEntry& result) {
    // Remove all carriage returns from input before parsing
    auto cleanedInput = input.substr(startPos);
    std::erase(cleanedInput, '\r');
    
    std::istringstream stream(cleanedInput);
    std::string token;
    std::string opCode;

    while (stream >> std::ws) {
        auto chr = static_cast<char>(stream.peek());
        if (chr == '"') {
            stream.get();
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
            // Skip move numbers (e.g., "1.", "23.")
            if (isMoveNumber(token)) {
                continue;
            }
            opCode = std::move(token);
        }
        else {
            result.operations[opCode].emplace_back(token);
        }
    }
}

} // namespace QaplaTester
