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
#include "epd-reader.h"
#include "pgn-io.h"

#include <algorithm>
#include <format>
#include <ranges>
#include <cctype>

namespace QaplaTester {

namespace {
    constexpr double MAX_ERROR_RATE = 0.2;
    constexpr size_t PROBE_GAME_COUNT = 100;
    constexpr size_t PROBE_LINE_COUNT = 100;
    constexpr size_t MAX_STORED_ERROR_LINES = 10;

    /**
     * @brief Checks if an EPD probe result is valid.
     * @param probeResult The probe result to check.
     * @param probeTrace Vector to add probe messages to.
     * @return true if probe passed, false if it failed.
     */
    bool checkEpdProbeResult(const EpdReaderResult& probeResult, std::vector<std::string>& probeTrace) {
        probeTrace.push_back(std::format("Probe: {} lines, {} entries, {} errors ({:.1f}% error rate)",
            PROBE_LINE_COUNT, probeResult.entries.size(), probeResult.errorCount, 
            probeResult.getErrorRate() * 100.0));
        
        if (probeResult.getTotalCount() == 0) {
            probeTrace.emplace_back("Probe failed: no lines processed");
            return false;
        }
        
        if (probeResult.getErrorRate() > MAX_ERROR_RATE) {
            probeTrace.push_back(std::format("Probe failed: error rate {:.1f}% exceeds maximum {:.1f}%",
                probeResult.getErrorRate() * 100.0, MAX_ERROR_RATE * 100.0));
            for (const auto& traceEntry : probeResult.getErrors()) {
                probeTrace.push_back(std::format("  {}", traceEntry.toString()));
            }
            return false;
        }
        
        return true;
    }

    /**
     * @brief Checks if a PGN probe result is valid.
     * @param probeResult The probe result to check.
     * @param probeTrace Vector to add probe messages to.
     * @return true if probe passed, false if it failed.
     */
    bool checkPgnProbeResult(const PgnReaderResult& probeResult, std::vector<std::string>& probeTrace) {
        probeTrace.push_back(std::format("Probe: {} games, {} errors ({:.1f}% error rate)",
            probeResult.games.size(), probeResult.errorCount, 
            probeResult.getErrorRate() * 100.0));
        
        if (!probeResult.fileOpened) {
            probeTrace.emplace_back("Probe failed: could not open file");
            return false;
        }
        
        if (probeResult.getTotalCount() == 0) {
            probeTrace.emplace_back("Probe failed: no games processed");
            return false;
        }
        
        if (probeResult.getErrorRate() > MAX_ERROR_RATE) {
            probeTrace.push_back(std::format("Probe failed: error rate {:.1f}% exceeds maximum {:.1f}%",
                probeResult.getErrorRate() * 100.0, MAX_ERROR_RATE * 100.0));
            for (const auto& entry : probeResult.trace) {
                probeTrace.push_back(std::format("  {}", entry.toString()));
            }
            return false;
        }
        
        return true;
    }
}

OpeningParser::OpeningParser() {
    registerEpdParser();
    registerPgnParser();
}

void OpeningParser::registerEpdParser() {
    addParser("EPD", {".epd", ".raw"}, [](const std::filesystem::path& filePath, 
                                          ParserTraceEntry& trace,
                                          const OpeningParserParams& params) -> std::optional<std::vector<GameRecord>> {
        try {
            // First probe with limited lines to check error rate
            EpdReader probeReader(filePath.string(), PROBE_LINE_COUNT, MAX_STORED_ERROR_LINES);
            auto probeResult = probeReader.getResult();
            
            std::vector<std::string> probeTrace;
            if (!checkEpdProbeResult(probeResult, probeTrace)) {
                // Add probe trace only on failure
                trace.messages.insert(trace.messages.end(), probeTrace.begin(), probeTrace.end());
                return std::nullopt;
            }
            
            // Read full file (or limited by maxGames)
            EpdReader fullReader(filePath.string(), params.maxGames, MAX_STORED_ERROR_LINES);
            auto fullResult = fullReader.getResult();
            
            trace.messages.push_back(std::format("Full scan: {} entries, {} errors ({:.1f}% error rate)",
                fullResult.entries.size(), fullResult.errorCount, fullResult.getErrorRate() * 100.0));
            
            if (fullResult.getTotalCount() == 0) {
                trace.messages.emplace_back("Full scan failed: no lines processed");
                return std::nullopt;
            }
            
            if (fullResult.getErrorRate() > MAX_ERROR_RATE) {
                trace.messages.push_back(std::format("Full scan failed: error rate {:.1f}% exceeds maximum {:.1f}%",
                    fullResult.getErrorRate() * 100.0, MAX_ERROR_RATE * 100.0));
                for (const auto& traceEntry : fullResult.getErrors()) {
                    trace.messages.push_back(std::format("  {}", traceEntry.toString()));
                }
                return std::nullopt;
            }
            
            // Convert EPD entries to GameRecords
            std::vector<GameRecord> result;
            result.reserve(fullResult.entries.size());
            for (const auto& entry : fullResult.entries) {
                GameRecord record;
                record.setStartPosition(false, entry.fen, true, 0);
                result.push_back(std::move(record));
            }
            
            trace.messages.push_back(std::format("Success: {} game records created", result.size()));
            trace.success = true;
            return result;
        } catch (const std::exception& exc) {
            trace.messages.push_back(std::format("Exception: {}", exc.what()));
            return std::nullopt;
        } catch (...) {
            trace.messages.emplace_back("Unknown exception");
            return std::nullopt;
        }
    });
}

void OpeningParser::registerPgnParser() {
    addParser("PGN", {".pgn"}, [](const std::filesystem::path& filePath,
                                  ParserTraceEntry& trace,
                                  const OpeningParserParams& params) -> std::optional<std::vector<GameRecord>> {
        try {
            PgnIO pgnIO;
            
            // First probe with limited games to check error rate
            PgnIO::LoadParams probeParams {
                .filePath = filePath.string(),
                .loadComments = true,
                .maxGames = PROBE_GAME_COUNT,
                .maxStoredErrorTraceEntries = MAX_STORED_ERROR_LINES,
            };
            
            auto probeResult = pgnIO.loadGamesWithResult(probeParams);
            
            std::vector<std::string> probeTrace;
            if (!checkPgnProbeResult(probeResult, probeTrace)) {
                // Add probe trace only on failure
                trace.messages.insert(trace.messages.end(), probeTrace.begin(), probeTrace.end());
                return std::nullopt;
            }
            
            // Read full file (or limited by maxGames)
            PgnIO::LoadParams fullParams {
                .filePath = filePath.string(),
                .loadComments = true,
                .maxGames = params.maxGames,
                .maxStoredErrorTraceEntries = MAX_STORED_ERROR_LINES
            };
            
            auto fullResult = pgnIO.loadGamesWithResult(fullParams);
            
            trace.messages.push_back(std::format("Full scan: {} games, {} errors ({:.1f}% error rate)",
                fullResult.games.size(), fullResult.errorCount, fullResult.getErrorRate() * 100.0));
            
            // Add trace entries from full scan (up to limit)
            size_t addedTraceEntries = 0;
            for (const auto& entry : fullResult.trace) {
                if (addedTraceEntries >= MAX_STORED_ERROR_LINES) {
                    break;
                }
                trace.messages.push_back(std::format("  {}", entry.toString()));
                ++addedTraceEntries;
            }
            
            if (fullResult.getTotalCount() == 0) {
                trace.messages.emplace_back("Full scan failed: no games processed");
                return std::nullopt;
            }
            
            if (fullResult.getErrorRate() > MAX_ERROR_RATE) {
                trace.messages.push_back(std::format("Full scan failed: error rate {:.1f}% exceeds maximum {:.1f}%",
                    fullResult.getErrorRate() * 100.0, MAX_ERROR_RATE * 100.0));
                return std::nullopt;
            }
            
            trace.messages.push_back(std::format("Success: {} games loaded", fullResult.games.size()));
            trace.success = true;
            return std::move(fullResult.games);
        } catch (const std::exception& exc) {
            trace.messages.push_back(std::format("Exception: {}", exc.what()));
            return std::nullopt;
        } catch (...) {
            trace.messages.emplace_back("Unknown exception");
            return std::nullopt;
        }
    });
}

void OpeningParser::addParser(const std::string& name, const std::vector<std::string>& extensions, const OpeningParserFunction& parser) {
    parsers_.push_back({
       .name = name, .extensions = extensions, .parser = parser
    });
}

std::vector<GameRecord> OpeningParser::parse(const std::filesystem::path& filePath, 
                                              std::optional<size_t> maxGames) const {
    auto result = parseWithTrace(filePath, maxGames);
    return result.games;
}

OpeningParserResult OpeningParser::parseWithTrace(const std::filesystem::path& filePath,
                                                   std::optional<size_t> maxGames) const {
    OpeningParserResult result;
    result.filePath = filePath.string();
    
    OpeningParserParams params;
    params.maxGames = maxGames;
    
    const auto extension = filePath.extension().string();
    
    // Helper to check if an extension matches (case-insensitive)
    auto matchesExtension = [&](const std::string& ext) {
        if (ext.length() != extension.length()) {
            return false;
        }
        return std::ranges::equal(ext, extension, [](char chr1, char chr2) {
            return std::tolower(static_cast<unsigned char>(chr1)) == std::tolower(static_cast<unsigned char>(chr2));
        });
    };

    // Helper to try a parser and record trace
    auto tryParser = [&](const ParserEntry& entry) -> bool {
        ParserTraceEntry traceEntry;
        traceEntry.parserName = entry.name;
        
        if (auto games = entry.parser(filePath, traceEntry, params); games) {
            result.games = std::move(*games);
            result.successfulParser = entry.name;
            traceEntry.success = true;
            result.trace.push_back(std::move(traceEntry));
            return true;
        }
        result.trace.push_back(std::move(traceEntry));
        return false;
    };

    // First try parsers with matching extension
    for (const auto& entry : parsers_) {
        for (const auto& ext : entry.extensions) {
            if (matchesExtension(ext)) {
                if (tryParser(entry)) {
                    return result;
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
            if (tryParser(entry)) {
                return result;
            }
        }
    }

    return result;
}

} // namespace QaplaTester
