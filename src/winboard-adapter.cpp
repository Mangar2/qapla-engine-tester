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


#include <cstring>
#include <iostream>
#include <sstream>
#include <chrono>
#include <sstream>
#include <limits>
#include <unordered_set>
#include "timer.h"
#include "string-helper.h"
#define WINBOARD
#ifdef WINBOARD
#include "winboard-adapter.h"
#include "engine-process.h"
#include "logger.h"

WinboardAdapter::WinboardAdapter(std::filesystem::path enginePath,
    const std::optional<std::filesystem::path>& workingDirectory,
    const std::string& identifier)
	: EngineAdapter(enginePath, workingDirectory, identifier)
{
    suppressInfoLines_ = true;
}

WinboardAdapter::~WinboardAdapter() {
    terminateEngine();
}

void WinboardAdapter::terminateEngine() {
	if (terminating_) {
		return; 
	}

    try {
        writeCommand("quit");
		// Once Terminating is set, writing to the engine is not allowed anymore
        terminating_ = true;
    }
    catch (...) {
        // Engine might already be gone; nothing to do
    }

    // Force termination if the engine didn't quit in time
    try {
        // Give the engine a short time to exit normally
        if (process_.waitForExit(engineQuitTimeout)) {
        }
        process_.terminate();
    }
    catch (const std::exception& ex) {
		Logger::testLogger().log("Failed to terminate engine (" + identifier_ + "): " + std::string(ex.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Failed to terminate engine (" + identifier_ + "): ", TraceLevel::error);
    }

}

void WinboardAdapter::startProtocol() {
    inFeatureSection_ = true;
    writeCommand("xboard");
}

void WinboardAdapter::newGame() {
    writeCommand("new");
}

void WinboardAdapter::moveNow() {
    writeCommand("?");
}

void WinboardAdapter::setPonder(bool enabled) {
	EngineAdapter::setPonder(enabled);
}

void WinboardAdapter::ticker() {
    // Currently unused in UCI
}

int64_t WinboardAdapter::allowPonder(
    [[maybe_unused]] const GameRecord & game, 
    [[maybe_unused]] const GoLimits & limits, 
    [[maybe_unused]] std::string ponderMove) {
    return 0;
}

int64_t WinboardAdapter::computeMove(
    [[maybe_unused]] const GameRecord& game,
    [[maybe_unused]] const GoLimits& limits,
    [[maybe_unused]] bool ponderHit) {
    return writeCommand("go");
}


std::string WinboardAdapter::computeGoOptions(const GoLimits& limits) const {
    std::ostringstream oss;
    if (limits.infinite) oss << " infinite";
    if (limits.movetimeMs) oss << " movetime " << *limits.movetimeMs;
    if (limits.depth) oss << " depth " << *limits.depth;
    if (limits.nodes) oss << " nodes " << *limits.nodes;
    if (limits.mateIn) oss << " mate " << *limits.mateIn;

    if (limits.wtimeMs > 0) oss << " wtime " << limits.wtimeMs;
    if (limits.btimeMs > 0) oss << " btime " << limits.btimeMs;
    if (limits.wincMs > 0)  oss << " winc " << limits.wincMs;
    if (limits.bincMs > 0)  oss << " binc " << limits.bincMs;

    if (limits.movesToGo > 0) oss << " movestogo " << limits.movesToGo;
	return oss.str();
}

void WinboardAdapter::askForReady() {
    pingCounter_++;
    writeCommand("ping " + std::to_string(pingCounter_));
}

void WinboardAdapter::sendPosition(const GameRecord& game) {
    writeCommand("force");

    if (game.getStartPos()) {
        writeCommand("new");
    }
    else {
        writeCommand("setboard " + game.getStartFen());
    }

    for (uint32_t ply = 0; ply < game.nextMoveIndex(); ++ply) {
        writeCommand("usermove " + game.history()[ply].lan);
    }
}

void WinboardAdapter::setTestOption(const std::string& name, const std::string& value) {
}

void WinboardAdapter::setOptionValues(const OptionValues& optionValues) {
    for (const auto& [name, value] : optionValues) {
        try {
			auto opt = getSupportedOption(name);
            if (!opt) {
                Logger::testLogger().log("Unsupported option: " + name, TraceLevel::info);
                continue;
            }
			auto supportedOption = *opt;
            // check type and  value constraints
            if (supportedOption.type == EngineOption::Type::String) {
                if (value.size() > 9999) {
                    Logger::testLogger().log("Option value for " + name + " is too long", TraceLevel::info);
                    continue;
                }
            }
            else if (supportedOption.type == EngineOption::Type::Spin) {
                int intValue = std::stoi(value);
                if (intValue < supportedOption.min || intValue > supportedOption.max) {
                    Logger::testLogger().log("Option value for " + name + " is out of bounds", TraceLevel::info);
                    continue;
                }
            }
			else if (supportedOption.type == EngineOption::Type::Check) {
				if (value != "true" && value != "false") {
					Logger::testLogger().log("Invalid boolean value for option " + name, TraceLevel::info);
					continue;
				}
			}
			else if (supportedOption.type == EngineOption::Type::Combo) {
				if (std::find(supportedOption.vars.begin(), supportedOption.vars.end(), value) == supportedOption.vars.end()) {
					Logger::testLogger().log("Invalid value for combo option " + name, TraceLevel::info);
					continue;
				}
			}
            std::string command = "setoption name " + supportedOption.name + " value " + value;
            writeCommand(command);
        }
        catch (...) {
            Logger::testLogger().log("Invalid value " + value + " for option " + name, TraceLevel::info);
        }

	}
}

/**
 * Tries to read an integer from stream, checks it against given bounds,
 * and stores it in the target if valid. Reports detailed errors otherwise.
 *
 * @param iss Token stream to read from.
 * @param fieldName Logical token name, used in error reporting.
 * @param min Minimum allowed value (inclusive).
 * @param max Maximum allowed value (inclusive).
 * @param target Optional to assign the result.
 * @param errors Vector collecting parse errors.
 * @return True if the value was read successfully no matter if in bounds, false otherwise.
 */
template <typename T>
bool readBoundedInt(std::istringstream& iss,
    const std::string& fieldName,
    T min,
    T max,
    std::optional<T>& target,
    std::vector<EngineEvent::ParseError>& errors)
{
    T value;
    if (!(iss >> value)) {
        errors.push_back({
            "missing-thinking-output",
            "Expected an integer for '" + fieldName + "'"
            });
        return false;
    }

    if (value < min || value > max) {
        errors.push_back({
            fieldName,
            "Reported value " + std::to_string(value) +
            " is outside the expected range [" +
            std::to_string(min) + ", " + std::to_string(max) + "]"
            });
        target = 0;
        return true;
    }

    target = value;
    return true;
}

template <typename T>
void storeBoundedInt(
    const std::string& token,
    const std::string& fieldName,
    T min,
    T max,
    std::optional<T>& target,
    std::vector<EngineEvent::ParseError>& errors)
{
    T value = std::stol(token);

    if (value < min || value > max) {
        errors.push_back({
            fieldName,
            "Reported value " + std::to_string(value) +
            " is outside the expected range [" +
            std::to_string(min) + ", " + std::to_string(max) + "]"
            });
		return;
    }

    target = static_cast<T>(value);
}

/**
 * Checks whether the next non-whitespace character in the stream is a tab.
 * Advances the stream to the tab if found.
 *
 * @param stream The input stream to inspect.
 * @return true if the next non-whitespace character is a tab, false otherwise.
 */
bool comesTab(std::istream& stream) {
    while (std::isspace(stream.peek()) && stream.peek() != '\t') {
        stream.get();
    }
    return stream.peek() == '\t';
}

std::vector<std::string> parseOptionalIntegers(std::istringstream& iss, EngineEvent& event) {
	std::vector<std::string> pv;
    std::vector<std::string> optionals;
    std::streampos pvStart = iss.tellg();
    std::string token;
    while (iss >> token) {
		pv.push_back(token);
		if (comesTab(iss)) {
            optionals = std::move(pv);
			pvStart = iss.tellg();
		}
    }
	if (!optionals.empty()) {
		const auto last = optionals.back();
		storeBoundedInt<int64_t>(last, "tbhits", 0, std::numeric_limits<int64_t>::max(), event.searchInfo->tbhits, event.errors);
	}
    if (optionals.size() > 1) {
        const auto selDepth = optionals[0];
		storeBoundedInt<int32_t>(selDepth, "seldepth", 0, 1000, event.searchInfo->selDepth, event.errors);
    }
    if (optionals.size() > 2) {
        const auto nps = optionals[1];
        storeBoundedInt<int64_t>(nps, "nps", 0, std::numeric_limits<int64_t>::max(), event.searchInfo->nps, event.errors);
    }
	iss.seekg(pvStart);
    iss >> std::ws;
    std::string pvText;
    std::getline(iss, pvText);
	event.searchInfo->pvText = pvText;

    return pv;
}

void parsePV(const std::vector<std::string>& pv, EngineEvent& event) {
    bool inParens = false;
    for (const auto& token : pv) {
        if (token.find('(') != std::string::npos) {
            inParens = true;
        }
        if (token.find(')') != std::string::npos) {
            inParens = false;
        }
        if (inParens) continue;
        if (std::isalpha(token[0]) || token == "0-0" || token == "0-0-0") {
			event.searchInfo->pv.push_back(token);
        }
    }
}

EngineEvent WinboardAdapter::parseSearchInfo(std::string depthStr, std::istringstream& iss, int64_t timestamp, const std::string& originalLine) {
    EngineEvent event = EngineEvent::createInfo(identifier_, timestamp, originalLine);

    event.searchInfo->depth = std::stoi(depthStr);

	if (!readBoundedInt<int32_t>(iss, "score", -110000, 110000, event.searchInfo->scoreCp, event.errors)) {
        return event;
	}
    if (*event.searchInfo->scoreCp <= -10000) event.searchInfo->scoreMate = *event.searchInfo->scoreCp + 10000;
	if (*event.searchInfo->scoreCp >= 10000) event.searchInfo->scoreMate = *event.searchInfo->scoreCp - 10000;

	if (!readBoundedInt<int64_t>(iss, "time", 0, std::numeric_limits<int64_t>::max() / 10, event.searchInfo->timeMs, event.errors)) {
		return event;
	}
    *event.searchInfo->timeMs *= 10;

	if (!readBoundedInt<int64_t>(iss, "nodes", 0, std::numeric_limits<int64_t>::max(), event.searchInfo->nodes, event.errors)) {
		return event;
	}

    // optionale ints (seldepth, nps, tbhits)
    auto pv = parseOptionalIntegers(iss, event);

    // principal variation
    parsePV(pv, event);

    return event;
}


EngineEvent WinboardAdapter::readFeatureSection(const EngineLine& engineLine) {
    const std::string& line = engineLine.content;
    if (line == "uciok") {
        logFromEngine(line, TraceLevel::command);
		inFeatureSection_ = false;
        return EngineEvent::createUciOk(identifier_, engineLine.timestampMs, line);
    }

    if (line.starts_with("id name ")) {
        logFromEngine(line, TraceLevel::info);
        engineName_ = line.substr(strlen("id name "));
    }

    if (line.starts_with("id author ")) {
        logFromEngine(line, TraceLevel::info);
        engineAuthor_ = line.substr(strlen("id author "));
    }

    if (line.starts_with("option ")) {
        logFromEngine(line, TraceLevel::info);
        try {
            EngineOption opt = parseUciOptionLine(line);
            supportedOptions_.push_back(std::move(opt));
        }
        catch (const std::exception& e) {
			EngineEvent event = EngineEvent::create(EngineEvent::Type::Error, identifier_, engineLine.timestampMs, line);
            std::string err = static_cast<std::string>("Bad uci option (") + e.what() + ")";
            return event;
        }
    }

    return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
}

EngineEvent WinboardAdapter::parseFeature(std::istringstream& iss, int64_t timestamp, const std::string& rawLine) {
    return EngineEvent::createNoData(identifier_, timestamp);
}

EngineEvent WinboardAdapter::readEvent() {
    EngineLine engineLine = process_.readLineBlocking();
    const std::string& line = engineLine.content;

    if (!engineLine.complete || engineLine.error == EngineLine::Error::IncompleteLine) {
        if (engineLine.complete) logFromEngine(line, TraceLevel::info);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (engineLine.error == EngineLine::Error::EngineTerminated) {
        if (terminating_) {
            return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
        }
        return EngineEvent::createEngineDisconnected(identifier_, engineLine.timestampMs, engineLine.content);
    }

    if (inFeatureSection_) {
        return readFeatureSection(engineLine);
    }

    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (isInteger(command)) {
        if (suppressInfoLines_) {
            return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
        }
        logFromEngine(line, TraceLevel::info);
        return parseSearchInfo(command, iss, engineLine.timestampMs, line);
    }

    if (command == "pong") {
        logFromEngine(line, TraceLevel::command);
        return EngineEvent::createReadyOk(identifier_, engineLine.timestampMs, line);
    }

    if (command == "Illegal" || command == "Error") {
        logFromEngine(line, TraceLevel::error);
        return EngineEvent::createError(identifier_, engineLine.timestampMs, line);
    }

    if (command == "move") {
        logFromEngine(line, TraceLevel::command);
        std::string move;
		iss >> move;
		return EngineEvent::createBestMove(identifier_, engineLine.timestampMs, line, move, "");
    }

    if (command == "tellics" || command == ".") {
        logFromEngine(line, TraceLevel::info);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (command == "hint") {
        logFromEngine(line, TraceLevel::info);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (command == "feature") {
        logFromEngine(line, TraceLevel::command);
        return parseFeature(iss, engineLine.timestampMs, line);
    }

    if (command == "resign") {
        logFromEngine(line, TraceLevel::command);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (command == "offer") {
        logFromEngine(line, TraceLevel::command);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (command == "tellusererror" || command == "tellallerror") {
        logFromEngine(line, TraceLevel::error);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (numUnknownCommandError_ <= 5) {
        logFromEngine(line + " Report: unknown command: ", TraceLevel::error);
        logFromEngine(line, TraceLevel::command);
        if (numUnknownCommandError_ == 5) {
            logFromEngine("Report: too many unknown command errors, stopping further checks", TraceLevel::error);
        }
        numUnknownCommandError_++;
    }

    return EngineEvent::createUnknown(identifier_, engineLine.timestampMs, line);
}

#endif