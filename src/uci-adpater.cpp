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
    const std::optional<std::filesystem::path>& workingDirectory)
	: EngineAdapter(enginePath, workingDirectory)
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
                _welcomeMessage += spacer + *line;
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
    writeCommand(std::string("setoption name Ponder value ") + (enabled ? "true" : "false"));
}

void UciAdapter::ticker() {
    // Currently unused in UCI
}

void UciAdapter::ponder(const GameRecord& game, GoLimits& limits) {
    // Pondering handled like normal search in UCI

}

int64_t UciAdapter::computeMove(const GameRecord& game, const GoLimits& limits) {
    sendPosition(game);

    std::ostringstream oss;
    oss << "go";

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

    // TODO: Add searchmoves

    return writeCommand(oss.str());
}


void UciAdapter::stopCalc() {
    writeCommand("stop");
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
        errors.push_back({ "search info " + fieldName, "Expected integer after '" + fieldName + "'"});
        iss.clear();
        return;
    }
    if (value < min || value > max) {
        errors.push_back({ "search info " + fieldName, 
            "Value out of range (" + std::to_string(min) + " to " + std::to_string(max) + "): " + std::to_string(value) });
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
    int value;
    if (!(iss >> value)) {
        errors.push_back({ "search info " + fieldName, "Expected integer after '" + fieldName + "'" });
        iss.clear();
        return;
    }
    if (value < min || value > max) {
        errors.push_back({ "search info " + fieldName,
            "Value out of range (" + std::to_string(min) + " to " + std::to_string(max) + "): " + std::to_string(value) });
        return;
    }
    target = value;
}

EngineEvent UciAdapter::parseSearchInfo(const std::string& line, int64_t timestamp) {
    SearchInfo info;
    EngineEvent event(EngineEvent::Type::Info, timestamp, line);
    std::istringstream iss(line);
    std::string token;
    iss >> token; // "info"

    while (iss >> token) {
        try {
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
                    event.errors.push_back({ "search info score", "Missing score type after 'score'" });
                    continue;
                }
                if (type == "cp") {
                    readBoundedInt32(iss, "score cp", -100000, 100000, info.scoreCp, event.errors);
                }
                else if (type == "mate") {
                    readBoundedInt32(iss, "score mate", -500, 500, info.scoreMate, event.errors);
                }
                else {
                    event.errors.push_back({ "search info score unknown type", "Unknown score type: '" + type + "'" });
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
                readBoundedInt32(iss, token, 0, 100, info.cpuload, event.errors);
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
                event.errors.push_back({ "search info unknown-token", "Unrecognized token: '" + token + "'" });
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
        return EngineEvent{ EngineEvent::Type::NoData, engineLine.timestampMs, "" };
    }

	const std::string& line = engineLine.content;

    if (line == "readyok") {
        logFromEngine(line, TraceLevel::handshake);
        return EngineEvent(EngineEvent::Type::ReadyOk, engineLine.timestampMs, line);
    }

    if (line.rfind("bestmove ", 0) == 0) {
        logFromEngine(line, TraceLevel::commands);
        std::istringstream iss(line);
        std::string token, best, ponder;
        iss >> token >> best;
        EngineEvent event(EngineEvent::Type::BestMove, engineLine.timestampMs, line);
        event.bestMove = best;
        if (iss >> token >> ponder && token == "ponder") {
            event.ponderMove = ponder;
        }
        return event;
    }

    if (line.rfind("info ", 0) == 0) {
        logFromEngine(line, TraceLevel::info);
        EngineEvent event = parseSearchInfo(line, engineLine.timestampMs);
        return event;
    }

    if (line == "ponderhit") {
        logFromEngine(line, TraceLevel::commands);
        return EngineEvent(EngineEvent::Type::PonderHit, engineLine.timestampMs, line);
    }

    // Unbekanntes Format
    logFromEngine(line, TraceLevel::error);
    return EngineEvent(EngineEvent::Type::Unknown, engineLine.timestampMs, line);
}

