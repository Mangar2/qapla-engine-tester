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


#include <iostream>
#include <sstream>
#include <chrono>
#include <sstream>
#include <limits>
#include <unordered_set>
#include "timer.h"

#include "uci-adapter.h"
#include "engine-process.h"
#include "logger.h"

UciAdapter::UciAdapter(std::filesystem::path enginePath,
    const std::optional<std::filesystem::path>& workingDirectory,
    const std::string& identifier)
	: EngineAdapter(enginePath, workingDirectory, identifier)
{
    suppressInfoLines_ = true;
}

UciAdapter::~UciAdapter() {
    terminateEngine();
}

void UciAdapter::restartEngine() {
    process_.restart();
}

void UciAdapter::terminateEngine() {
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

    // Give the engine a short time to exit normally
    if (process_.waitForExit(engineQuitTimeout)) {
    }

    // Force termination if the engine didn't quit in time
    try {
        process_.terminate();
    }
    catch (const std::exception& ex) {
		Logger::testLogger().log("Failed to terminate engine (" + identifier_ + "): " + std::string(ex.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Failed to terminate engine (" + identifier_ + "): ", TraceLevel::error);
    }

}

void UciAdapter::startProtocol() {
	inUciHandshake_ = true;
	writeCommand("uci");
}

void UciAdapter::newGame() {
    writeCommand("ucinewgame");
}

void UciAdapter::moveNow() {
    writeCommand("stop");
}

void UciAdapter::setPonder(bool enabled) {
	EngineAdapter::setPonder(enabled);
	if (supportedOptions_.find("Ponder") == supportedOptions_.end()) {
		return; // Ponder option not supported
	}
    writeCommand(std::string("setoption name Ponder value ") + (enabled ? "true" : "false"));
}

void UciAdapter::ticker() {
    // Currently unused in UCI
}

int64_t UciAdapter::allowPonder(const GameRecord & game, const GoLimits & limits, std::string ponderMove) {
    if (ponderMove == "") return 0;
	sendPosition(game, ponderMove);

    std::ostringstream oss;
    oss << "go ponder" << computeGoOptions(limits);

    return writeCommand(oss.str());
}

int64_t UciAdapter::computeMove(const GameRecord& game, const GoLimits& limits, bool ponderHit) {
    if (ponderHit) {
		return writeCommand("ponderhit");
    }
    sendPosition(game);

    std::ostringstream oss;
    oss << "go" << computeGoOptions(limits);

    return writeCommand(oss.str());
}

std::string UciAdapter::computeGoOptions(const GoLimits& limits) const {
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

void UciAdapter::askForReady() {
	writeCommand("isready");
}

void UciAdapter::sendPosition(const GameRecord& game, std::string ponderMove) {
    std::ostringstream oss;
    if (game.getStartPos()) {
        oss << "position startpos";
    }
    else {
        oss << "position fen " << game.getStartFen();
    }
    if (!game.nextMoveIndex() == 0 || ponderMove != "") {
        oss << " moves";
		for (uint32_t ply = 0; ply < game.nextMoveIndex(); ++ply) {
            oss << " " << game.history()[ply].lan;
		}
        if (ponderMove != "") {
            oss << " " << ponderMove;
        }
    }
    writeCommand(oss.str());
}

void UciAdapter::setTestOption(const std::string& name, const std::string& value) {
    std::string command = "setoption name " + name + " value " + value;
    writeCommand(command);
}

void UciAdapter::setOptionValues(const OptionValues& optionValues) {
    for (const auto& [name, value] : optionValues) {
        try {
            if (supportedOptions_.find(name) == supportedOptions_.end()) {
                Logger::testLogger().log("Unsupported option: " + name, TraceLevel::info);
                continue;
            }
			auto supportedOption = supportedOptions_.at(name);
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
            std::string command = "setoption name " + name + " value " + value;
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
 */
void readBoundedInt32(std::istringstream& iss,
    const std::string& fieldName,
    int32_t min,
    int32_t max,
    std::optional<int>& target,
    std::vector<EngineEvent::ParseError>& errors)
{
    int value;
    if (!(iss >> value)) {
        errors.push_back({
            "Search info reports correct  " + fieldName,
            "Expected an integer after '" + fieldName + "'"
            });
        iss.clear();
        return;
    }

    if (value < min || value > max) {
        errors.push_back({
            "Search info reports correct " + fieldName,
            "Reported value " + std::to_string(value) +
            " is outside the expected range [" +
            std::to_string(min) + ", " + std::to_string(max) + "]"
            });
        return;
    }
    if (target.has_value()) {
        errors.push_back({ "duplicate-info-field", "Field '" + fieldName + "' specified more than once" });
        return;
    }
    target = value;
}


void readBoundedInt64(std::istringstream& iss,
    const std::string& fieldName,
    int64_t min,
    int64_t max,
    std::optional<int64_t>& target,
    std::vector<EngineEvent::ParseError>& errors)
{
    int64_t value;
    if (!(iss >> value)) {
        errors.push_back({
            "Search info reports correct " + fieldName,
            "Expected an integer after '" + fieldName + "'"
            });
        iss.clear();
        return;
    }

    if (value < min || value > max) {
        errors.push_back({
            "Search info reports correct " + fieldName,
            "Reported value " + std::to_string(value) +
            " is outside the expected range [" +
            std::to_string(min) + ", " + std::to_string(max) + "]"
            });
        return;
    }
    if (target.has_value()) {
        errors.push_back({ "duplicate-info-field", "Field '" + fieldName + "' specified more than once" });
        return;
    }

    target = value;
}

bool isLanMoveToken(const std::string& token) {
	// A valid LAN move token is a string of 4 or 5 characters, starting with a letter
	// and followed by 3 or 4 digits (e.g., "e2e4", "g1f3", "d7d5").
	if (token.size() < 4 || token.size() > 5) return false;
	if (token[0] < 'a' || token[0] > 'h') return false; // First character must be a letter a-h
	if (token[1] < '1' || token[1] > '8') return false; // Second character must be a digit 1-8
	if (token[2] < 'a' || token[2] > 'h') return false; // Third character must be a letter a-h
	if (token[3] < '1' || token[3] > '8') return false; // Fourth character must be a digit 1-8
	return true;
}

EngineEvent UciAdapter::parseSearchInfo(std::istringstream& iss, int64_t timestamp, const std::string& rawLine) {
    SearchInfo info;
    EngineEvent event = EngineEvent::create(EngineEvent::Type::Info, identifier_, timestamp, rawLine);
    std::string token;
    std::string parent;

    auto checkDuplicateField = [&](bool check, const std::string& fieldName) {
        if (check) {
            event.errors.push_back({ "duplicate-info-field", "Field '" + fieldName + "' specified more than once" });
        };
        return check;
        };
    // Checks and reports duplicate field usage
    // Fields that require sub-tokens (e.g., score, pv) are tracked via 'parent'
    //
    // Special handling:
    // - score accepts cp|mate [lowerbound|upperbound]
    // - tokens after score are parsed until a non-score token is found
    // - tokens after pv/refutation/currline/currmove are captured based on parent state
    //
    // Remaining tokens are matched against known keywords or reported as errors
    while (iss >> token) {
        try {
            if (parent == "score") {
                if (token == "cp") readBoundedInt32(iss, "score cp", -100000, 100000, info.scoreCp, event.errors);
                else if (token == "mate") readBoundedInt32(iss, "score mate", -500, 500, info.scoreMate, event.errors);
                else if (token == "lowerbound") info.scoreLowerbound = true;
                else if (token == "upperbound") info.scoreUpperbound = true;
                else parent = ""; // terminate score parsing, allow re-processing of current token
                if (parent == "score") continue;
            }
            if (isLanMoveToken(token)) {
                if (parent == "currmove") {
                    info.currMove = token;
                    parent = "";
                }
                else if (parent == "pv") { info.pv.push_back(token); }
                else if (parent == "refutation") { info.refutation.push_back(token); continue; }
                else if (parent == "currline") { info.currline.push_back(token); continue; }
				else {
					// If we encounter a move token without a parent, it is an error
					event.errors.push_back({ "unexpected-move-token", "Unexpected move token '" + token + "' without context" });
				}
                continue;
            }
            if (token == "string") {
                std::string restOfLine;
                std::getline(iss >> std::ws, restOfLine);
                event.stringInfo = restOfLine;
            }
            else if (token == "score") {}
            else if (token == "currmove") checkDuplicateField(info.currMove.has_value(), token);
            else if (token == "pv") checkDuplicateField(info.pv.size() > 0, token);
			else if (token == "refutation") checkDuplicateField(info.refutation.size() > 0, token);
			else if (token == "currline") checkDuplicateField(info.currline.size() > 0, token);
            // ReadBoundInt checks for duplicate field. 
            else if (token == "depth") readBoundedInt32(iss, token, 0, 1000, info.depth, event.errors);
            else if (token == "seldepth") readBoundedInt32(iss, token, 0, 1000, info.selDepth, event.errors);
            else if (token == "multipv") readBoundedInt32(iss, token, 1, 220, info.multipv, event.errors);
            else if (token == "time") readBoundedInt64(iss, token, 0, std::numeric_limits<int64_t>::max(), info.timeMs, event.errors);
            else if (token == "nodes") readBoundedInt64(iss, token, 0, std::numeric_limits<int64_t>::max(), info.nodes, event.errors);
            else if (token == "nps") readBoundedInt64(iss, token, 0, std::numeric_limits<int64_t>::max(), info.nps, event.errors);
            else if (token == "hashfull") readBoundedInt32(iss, token, 0, 1000, info.hashFull, event.errors);
            else if (token == "tbhits") readBoundedInt32(iss, token, 0, std::numeric_limits<int>::max(), info.tbhits, event.errors);
            else if (token == "sbhits") readBoundedInt32(iss, token, 0, std::numeric_limits<int>::max(), info.sbhits, event.errors);
            else if (token == "cpuload") readBoundedInt32(iss, token, 0, 1000, info.cpuload, event.errors);
            else if (token == "currmovenumber") readBoundedInt32(iss, token, 1, std::numeric_limits<int>::max(), info.currMoveNumber, event.errors);
            else {
                event.errors.push_back({ "Wrong token in info line", "Unrecognized or misplaced token: '" + token + "' after " + parent });
            }
            parent = token;
        }
        catch (const std::exception& e) {
            event.errors.push_back({ "parsing-exception", e.what() });
        }
    }

    event.searchInfo = std::move(info);
    return event;
}


EngineEvent UciAdapter::readUciEvent(const EngineLine& engineLine) {
    const std::string& line = engineLine.content;
    if (line == "uciok") {
        logFromEngine(line, TraceLevel::command);
		inUciHandshake_ = false;
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
            supportedOptions_[opt.name] = std::move(opt);
        }
        catch (const std::exception& e) {
			EngineEvent event = EngineEvent::create(EngineEvent::Type::Error, identifier_, engineLine.timestampMs, line);
            std::string err = static_cast<std::string>("Bad uci option (") + e.what() + ")";
            return event;
        }
    }

    return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
}

EngineEvent UciAdapter::readEvent() {
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

	if (inUciHandshake_) {
		return readUciEvent(engineLine);
	}

	// std::cout << identifier_ << "-> " << line << std::endl; // Debug output

    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (command == "info") {
		if (suppressInfoLines_) {
			return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
		}
        logFromEngine(line, TraceLevel::info);
        return parseSearchInfo(iss, engineLine.timestampMs, line);
    }

    if (command == "readyok") {
        logFromEngine(line, TraceLevel::command);
        return EngineEvent::createReadyOk(identifier_, engineLine.timestampMs, line);
    }
	if (command == "uciok") {
		logFromEngine(line, TraceLevel::command);
		return EngineEvent::createUciOk(identifier_, engineLine.timestampMs, line);
	}

    if (command == "bestmove") {
        logFromEngine(line, TraceLevel::command);
        std::string best, token, ponder, err;
        iss >> best;
        iss >> token;
        if (token == "ponder") {
            iss >> ponder;
        }
        else {
            ponder = "";
        }
        EngineEvent e = EngineEvent::createBestMove(identifier_, engineLine.timestampMs, line, best, ponder);
        if (token != "ponder" && token != "") {
            err = "Expected 'ponder' or nothing after bestmove, got '" + token + "'";
            e.errors.push_back({ "bestmove", err });
        }
        return e;
    }

    if (command == "ponderhit") {
        logFromEngine(line, TraceLevel::command);
        return EngineEvent::createPonderHit(identifier_, engineLine.timestampMs, line);
    }

    if (command == "option") {
        if (numOptionError_ <= 5) {
            logFromEngine(line + " Report: option command outside uci/uciok: ", TraceLevel::error);
            logFromEngine(line, TraceLevel::command);
            if (numOptionError_ == 5) {
                logFromEngine("Report: too many option errors, stopping further checks", TraceLevel::error);
            }
            numOptionError_++;
        }
    }
    else if (command == "id") {
        if (numIdError_ <= 5) {
            logFromEngine(line + " Report: id name command outside uci/uciok: ", TraceLevel::error);
            logFromEngine(line, TraceLevel::command);
            if (numIdError_ == 5) {
                logFromEngine("Report: too many id name errors, stopping further checks", TraceLevel::error);
            }
            numIdError_++;
        }
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }
    else if (command == "name") {
        if (numNameError_ <= 5) {
            logFromEngine(line + " Report: name command outside uci/uciok: ", TraceLevel::error);
            logFromEngine(line, TraceLevel::command);
            if (numNameError_ == 5) {
                logFromEngine("Report: too many name errors, stopping further checks", TraceLevel::error);
            }
            numNameError_++;
        }
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
