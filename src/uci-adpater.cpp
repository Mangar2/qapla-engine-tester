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
#include "timer.h"

#include "uci-adapter.h"
#include "engine-process.h"

UciAdapter::UciAdapter(std::filesystem::path enginePath,
    const std::optional<std::filesystem::path>& workingDirectory)
	: EngineAdapter(enginePath, workingDirectory)
{
}

UciAdapter::~UciAdapter() {
    terminateEngine();
}

void UciAdapter::skipLines(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        auto line = process_.readLine(timeout);
        if (!line) continue;
		logFromEngine(*line);
    }
}

void UciAdapter::runUciHandshake() {
    writeCommand("uci");

    while (true) {
        auto line = process_.readLine(uciHandshakeTimeout);
        if (!line) {
			reportProtocolError("initialization", "Timeout waiting for uciok");
            throw std::runtime_error("Engine is not an uci engine");
            break; 
        }
        logFromEngine(*line);

        if (*line == "uciok") {
            break;
        }

        if (line->starts_with("id name ")) {
            engineName_ = line->substr(strlen("id name "));
        }
        else if (line->starts_with("id author ")) {
            engineAuthor_ = line->substr(strlen("id author "));
        }
        else if (line->starts_with("option ")) {
            try {
                UciOption opt = parseUciOptionLine(*line);
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
        skipLines(engineIntroScanDuration);
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
        process_.writeLine("quit");
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

void UciAdapter::newGame(const GameStartPosition& position) {
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

void UciAdapter::ponder(const GameState& game, GoLimits& limits) {
    // Pondering handled like normal search in UCI

}

void UciAdapter::computeMove(const GameState& game, const GoLimits& limits) {
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

    writeCommand(oss.str());
}


void UciAdapter::stopCalc() {
    writeCommand("stop");
}

void UciAdapter::writeCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(commandMutex_);
	logToEngine(command);
    process_.writeLine(command);
}

const OptionMap& UciAdapter::getOptionMap() const {
    return options_;
}

void UciAdapter::setOptionMap(const OptionMap& list) {
    options_ = list;
    for (const auto& [key, value] : list) {
        std::ostringstream oss;
        oss << "setoption name " << key << " value " << value;
        writeCommand(oss.str());
    }
}

void UciAdapter::askForReady() {
	writeCommand("isready");
}

void UciAdapter::sendPosition(const GameState& game) {
    std::ostringstream oss;
    if (game.startPosition().fen().empty()) {
        oss << "position startpos";
    }
    else {
        oss << "position fen " << game.startPosition().fen();
    }
    if (!game.moveList().empty()) {
        oss << " moves";
        for (const auto& move : game.moveList()) {
            oss << " " << move;
        }
    }
    writeCommand(oss.str());
}

EngineEvent UciAdapter::readEvent() {
    EngineLine engineLine = process_.readLineBlocking();
    if (engineLine.content == "") {
        return EngineEvent{ EngineEvent::Type::NoData, engineLine.timestampMs, "" };
    }

	const std::string& line = engineLine.content;

    logFromEngine(line);
    if (line == "readyok") {
        return EngineEvent(EngineEvent::Type::ReadyOk, engineLine.timestampMs, line);
    }

    if (line.rfind("bestmove ", 0) == 0) {
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
        // Option 1: nur weiterreichen
        EngineEvent event(EngineEvent::Type::Info, engineLine.timestampMs, line);
        // Option 2: parse into SearchInfo (siehe vorherige Vorschläge)
        return event;
    }

    if (line == "ponderhit") {
        return EngineEvent(EngineEvent::Type::PonderHit, engineLine.timestampMs, line);
    }

    // Unbekanntes Format
    return EngineEvent(EngineEvent::Type::Unknown, engineLine.timestampMs, line);
}

