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
#include "timer.h"

#include "uci-adapter.h"
#include "engine-process.h"
#include "logger.h"

UciAdapter::UciAdapter(std::filesystem::path enginePath,
    const std::optional<std::filesystem::path>& workingDirectory,
    const std::string& identifier)
	: EngineAdapter(enginePath, workingDirectory, identifier)
{
}

UciAdapter::~UciAdapter() {
    terminateEngine();
}

void UciAdapter::runUciHandshake() {
    writeCommand("uci");
    bool headerParsed = false;
    std::string spacer = "";
    while (true) {
        auto line = process_.readLine(uciHandshakeTimeout);
        if (!line) {
            reportProtocolError("initialization", "Timeout waiting for uciok");
            throw std::runtime_error("Engine is not an uci engine");
        }

        logFromEngine(*line, TraceLevel::handshake);

        if (*line == "uciok") {
            break;
        }

        if (!headerParsed) {
            if (line->starts_with("id ") || line->starts_with("option ")) {
                headerParsed = true;
            }
            else {
                welcomeMessage_ += spacer + *line;
				spacer = "\n";
                continue;
            }
        }

        if (line->starts_with("id name ")) {
            engineName_ = line->substr(strlen("id name "));
        }
        else if (line->starts_with("id author ")) {
            engineAuthor_ = line->substr(strlen("id author "));
        }
        else if (line->starts_with("option ")) {
            try {
                EngineOption opt = parseUciOptionLine(*line);
                supportedOptions_[opt.name] = std::move(opt);
            }
            catch (const std::exception& e) {
                reportProtocolError("initialization", *line + " (" + e.what() + ")");
            }
        }
        else {
            reportProtocolError("initialization", "Unexpected line during UCI handshake: " + *line);
        }
    }
}


void UciAdapter::runEngine() {
    try {
        runUciHandshake();
		state_ = EngineState::Initialized;
    }
    catch (const std::exception& e) {
        reportProtocolError("initialization", std::string("Failed to start UCI engine:  ") + e.what());
    }
}

void UciAdapter::terminateEngine() {
	if (state_ == EngineState::Terminating) {
		return; // Already terminating
	}
    state_ = EngineState::Terminating;

    try {
        writeCommand("quit");
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
        reportProtocolError("termination", std::string("Termination error: ") + ex.what());
    }

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

void UciAdapter::ponder(const GameRecord& game, GoLimits& limits) {
    

}

int64_t UciAdapter::computeMove(const GameRecord& game, const GoLimits& limits) {
    sendPosition(game);

    std::ostringstream oss;
    oss << "go" << computeGoOptions(limits);

    // TODO: Add searchmoves
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

void UciAdapter::sendPosition(const GameRecord& game) {
    std::ostringstream oss;
    if (game.getStartPos()) {
        oss << "position startpos";
    }
    else {
        oss << "position fen " << game.getStartFen();
    }
    if (!game.currentPly() == 0) {
        oss << " moves";
		for (uint32_t ply = 0; ply < game.currentPly(); ++ply) {
            oss << " " << game.history()[ply].lan;
		}
    }
    writeCommand(oss.str());
}

void UciAdapter::setOption(const std::string& name, const std::string& value) {
    std::string command = "setoption name " + name;
    if (!value.empty()) {
        command += " value " + value;
    }
    writeCommand(command);
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
            "Search info reports correct  " + fieldName,
            "Reported value " + std::to_string(value) +
            " is outside the expected range [" +
            std::to_string(min) + ", " + std::to_string(max) + "]"
            });
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

    target = value;
}

EngineEvent UciAdapter::parseSearchInfo(const std::string& line, int64_t timestamp) {
    SearchInfo info;
    EngineEvent event = EngineEvent::create(EngineEvent::Type::Info, identifier_, timestamp, line);
    std::istringstream iss(line);
    std::string token;
    iss >> token; // "info"

    while (iss >> token) {
        try {
            if (token == "string") {
                break;
            }
            if (token == "depth") {
                readBoundedInt32(iss, token, 0, 1000, info.depth, event.errors);
            }
            else if (token == "seldepth") {
                readBoundedInt32(iss, token, 0, 1000, info.selDepth, event.errors);
            }
            else if (token == "multipv") {
                readBoundedInt32(iss, token, 1, 220, info.multipv, event.errors);
            }
            else if (token == "score") {
                std::string type;
                if (!(iss >> type)) {
                    event.errors.push_back({ "Search info reports correct score", "Missing score type after 'score'" });
                    continue;
                }
                if (type == "cp") {
                    readBoundedInt32(iss, "score cp", -100000, 100000, info.scoreCp, event.errors);
                }
                else if (type == "mate") {
                    readBoundedInt32(iss, "score mate", -500, 500, info.scoreMate, event.errors);
                }
                else {
                    event.errors.push_back({ "search info reports only specified fields", "Unknown score type: '" + type + "'" });
                    continue;
                }

                std::string bound;
                if (iss >> bound) {
                    if (bound == "lowerbound") info.scoreLowerbound = true;
                    else if (bound == "upperbound") info.scoreUpperbound = true;
                    else iss.seekg(-static_cast<int>(bound.size()), std::ios_base::cur);
                }
            }
            else if (token == "time") {
                readBoundedInt64(iss, token, 0, std::numeric_limits<int64_t>::max(), info.timeMs, event.errors);
            }
            else if (token == "nodes") {
                readBoundedInt64(iss, token, 0, std::numeric_limits<int64_t>::max(), info.nodes, event.errors);
            }
            else if (token == "nps") {
                readBoundedInt64(iss, token, 0, std::numeric_limits<int64_t>::max(), info.nps, event.errors);
            }
            else if (token == "hashfull") {
                readBoundedInt32(iss, token, 0, 1000, info.hashFull, event.errors);
            }
            else if (token == "tbhits") {
                readBoundedInt32(iss, token, 0, std::numeric_limits<int>::max(), info.tbhits, event.errors);
            }
            else if (token == "cpuload") {
                readBoundedInt32(iss, token, 0, 1000, info.cpuload, event.errors);
            }
            else if (token == "currmove") {
                std::string move;
                if (iss >> move) info.currMove = move;
                else event.errors.push_back({ "search info currmove", "Expected move string after 'currmove'" });
            }
            else if (token == "currmovenumber") {
                readBoundedInt32(iss, token, 0, std::numeric_limits<int>::max(), info.currMoveNumber, event.errors);
            }
            else if (token == "refutation") {
                event.errors.push_back({ "search info refutation", "Refutation parsing not yet supported" });
            }
            else if (token == "pv") {
                std::string move;
                while (iss >> move) {
                    info.pv.push_back(move);
                }
                break; // PV ends the line
            }
            else {
                event.errors.push_back({ "search info unknown-token", "Unrecognized or misplaced token: '" + token + "' (raw line: " + line + ")"});
            }
        }
        catch (const std::exception& e) {
            event.errors.push_back({ "search info exception", "Exception during parsing: " + std::string(e.what()) });
        }
    }

    event.searchInfo = info;
    return event;
}

const std::string testLine =
"info depth xx seldepth -5 multipv 0 score xyz 999999 bounder time -1 nodes abc nps 999999999999999999999 "
"hashfull 2000 tbhits foo cpuload 150 currmove 1234 currmovenumber -42 refutation e2e4 e7e5 unknown_token pv e2e4 e7e5";

EngineEvent UciAdapter::readEvent() {
    EngineLine engineLine = process_.readLineBlocking();
    if (engineLine.content == "") {
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    const std::string& line = engineLine.content;
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (command == "readyok") {
        logFromEngine(line, TraceLevel::handshake);
        return EngineEvent::createReadyOk(identifier_, engineLine.timestampMs, line);
    }

    if (command == "bestmove") {
        logFromEngine(line, TraceLevel::commands);
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

    if (command == "info") {
        logFromEngine(line, TraceLevel::info);
        return parseSearchInfo(line, engineLine.timestampMs);
    }

    if (command == "ponderhit") {
        logFromEngine(line, TraceLevel::commands);
        return EngineEvent::createPonderHit(identifier_, engineLine.timestampMs, line);
    }

    if (command == "option") {
        if (numOptionError_ <= 5) {
            logFromEngine(line + " Report: option command outside uci/uciok: ", TraceLevel::error);
            logFromEngine(line, TraceLevel::commands);
            if (numOptionError_ == 5) {
                logFromEngine("Report: too many option errors, stopping further checks", TraceLevel::error);
            }
            numOptionError_++;
        }
    }
    else if (command == "id") {
        if (numIdError_ <= 5) {
            logFromEngine(line + " Report: id name command outside uci/uciok: ", TraceLevel::error);
            logFromEngine(line, TraceLevel::commands);
            if (numIdError_ == 5) {
                logFromEngine("Report: too many id name errors, stopping further checks", TraceLevel::error);
            }
            numIdError_++;
        }
    }
    else if (command == "name") {
        if (numNameError_ <= 5) {
            logFromEngine(line + " Report: name command outside uci/uciok: ", TraceLevel::error);
            logFromEngine(line, TraceLevel::commands);
            if (numNameError_ == 5) {
                logFromEngine("Report: too many name errors, stopping further checks", TraceLevel::error);
            }
            numNameError_++;
        }
    }

    if (numUnknownCommandError_ <= 5) {
        logFromEngine(line + " Report: unknown command: ", TraceLevel::error);
        logFromEngine(line, TraceLevel::commands);
        if (numUnknownCommandError_ == 5) {
            logFromEngine("Report: too many unknown command errors, stopping further checks", TraceLevel::error);
        }
        numUnknownCommandError_++;
    }
    return EngineEvent::createUnknown(identifier_, engineLine.timestampMs, line);
}
