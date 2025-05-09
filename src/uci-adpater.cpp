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

#include "uci-adapter.h"

#include <iostream>
#include <sstream>
#include <chrono>

UciAdapter::UciAdapter(std::filesystem::path enginePath,
    const std::optional<std::filesystem::path>& workingDirectory)
    : process_(enginePath, workingDirectory) {
}

UciAdapter::~UciAdapter() {
    terminateEngine();
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
        std::cout << "[ENGINE] " << *line << std::endl;

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
        runUciHandshake();
		state_ = UciState::Initialized;
    }
    catch (const std::exception& e) {
        reportProtocolError("initialization", std::string("Failed to start UCI engine:  ") + e.what());
    }
}

void UciAdapter::terminateEngine() {
    state_ = UciState::Terminating;

    try {
        process_.writeLine("quit");
    }
    catch (...) {
        // Engine might already be gone; nothing to do
    }

    // Give the engine a short time to exit normally
    if (process_.waitForExit(engineQuitTimeout)) {
        if (readerThread_.joinable()) {
            readerThread_.join();
        }
        return;
    }

    // Force termination if the engine didn't quit in time
    try {
        process_.terminate();
        reportProtocolError("termination", "Engine did not terminate after quit");
    }
    catch (const std::exception& ex) {
        reportProtocolError("termination", std::string("Failed to force-terminate engine: ") + ex.what());
    }

    if (readerThread_.joinable()) {
        readerThread_.join();
    }
}

void UciAdapter::newGame(const GameStartPosition& position) {
    writeCommand("ucinewgame");
    waitForReady();
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

void UciAdapter::calcMove(const GameState& game, GoLimits& limits, const MoveList& limitMoves) {
    sendPosition(game);

    std::ostringstream oss;
    oss << "go";

    if (limits.infinite) oss << " infinite";
    if (limits.movetimeMs) oss << " movetime " << *limits.movetimeMs;
    if (limits.depth) oss << " depth " << *limits.depth;
    if (limits.nodes) oss << " nodes " << *limits.nodes;
    if (limits.mateIn) oss << " mate " << *limits.mateIn;

    oss << " wtime " << limits.wtimeMs
        << " btime " << limits.btimeMs
        << " winc " << limits.wincMs
        << " binc " << limits.bincMs;

    if (limits.movesToGo > 0) oss << " movestogo " << limits.movesToGo;

    writeCommand(oss.str());
}

void UciAdapter::stopCalc() {
    writeCommand("stop");
}

void UciAdapter::writeCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(commandMutex_);
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
    waitForReady();
}

void UciAdapter::waitForReady() {
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

void UciAdapter::readerLoop() {
    while (process_.isRunning()) {
        if (auto line = process_.readLine(std::chrono::milliseconds(100))) {
            std::cout << "[ENGINE] " << *line << std::endl;
        }
    }
}