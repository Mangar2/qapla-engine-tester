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
#include <limits>
#include <unordered_set>
#include "timer.h"
#include "string-helper.h"
#include "winboard-adapter.h"
#include "engine-process.h"
#include "logger.h"

namespace QaplaTester {

WinboardAdapter::WinboardAdapter(const std::filesystem::path& enginePath,
    const std::optional<std::filesystem::path>& workingDirectory,
    const std::string& identifier)
	: EngineAdapter(enginePath, workingDirectory, identifier)
{
    suppressInfoLines_ = true;
}

WinboardAdapter::~WinboardAdapter() {
    WinboardAdapter::terminateEngine();
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
    writeCommand("protover 2");
}

void WinboardAdapter::newGame(const GameRecord& gameRecord, bool engineIsWhite) {
    gameStruct_ = gameRecord.createGameStruct();
    sendPosition(gameStruct_);
    const auto& timeControl = engineIsWhite ? 
        gameRecord.getWhiteTimeControl() : 
        gameRecord.getBlackTimeControl();

	setTimeControl(timeControl);
    lastOwnMove_.clear();
    // Post mode shows thinking output
    writeCommand("post");
}

void WinboardAdapter::setTimeControl(const GameRecord& gameRecord, bool engineIsWhite) {
    const auto& timeControl = engineIsWhite ? 
        gameRecord.getWhiteTimeControl() : 
        gameRecord.getBlackTimeControl();

	setTimeControl(timeControl);
}

void WinboardAdapter::setTimeControl(const TimeControl& timeControl) {
    if (!timeControl.isValid()) {
        return;
    }


    const auto& segments = timeControl.timeSegments();
    if (!segments.empty()) {
        const auto& seg = segments[0];
        int moves = seg.movesToPlay > 0 ? seg.movesToPlay : 0;
        int baseSeconds = static_cast<int>(seg.baseTimeMs / 1000);
		int baseMinutes = baseSeconds / 60;
		baseSeconds %= 60;
        int incSeconds = static_cast<int>(seg.incrementMs / 1000);
        std::ostringstream oss;
        if (!clearTimeControlCommand_.empty()) {
            writeCommand(clearTimeControlCommand_);
            clearTimeControlCommand_.clear();
        }
        oss << "level " << moves << " "
            << baseMinutes << ":"
            << baseSeconds << " "
			<< incSeconds;
        writeCommand(oss.str());
        return;
    }

    if (timeControl.moveTimeMs()) {
        if (!clearTimeControlCommand_.empty() && clearTimeControlCommand_ != "st 0") {
            writeCommand(clearTimeControlCommand_);
            clearTimeControlCommand_.clear();
        }
        clearTimeControlCommand_ = "st 0";
        writeCommand("st " + std::to_string(static_cast<int>(*timeControl.moveTimeMs() / 1000)));
        return;
    }

    if (timeControl.depth()) {
        if (!clearTimeControlCommand_.empty() && clearTimeControlCommand_ != "sd 0") {
            writeCommand(clearTimeControlCommand_);
            clearTimeControlCommand_.clear();
        }
        clearTimeControlCommand_ = "sd 0";
        writeCommand("sd " + std::to_string(*timeControl.depth()));
        return;
    }

    if (timeControl.nodes()) {
        if (!clearTimeControlCommand_.empty() && clearTimeControlCommand_ != "nps 0") {
            writeCommand(clearTimeControlCommand_);
            clearTimeControlCommand_.clear();
        }
        clearTimeControlCommand_ = "nps 0";
        writeCommand("nps " + std::to_string(*timeControl.nodes()));
        return;
    }
}

void WinboardAdapter::setForceMode() {
    if (forceMode_) {
        return;
    }
    writeCommand("force");
    forceMode_ = true;
}

void WinboardAdapter::moveNow() {
    if (forceMode_) {
        // If we are in force mode, the engine is not doing anything and cannot move now.
        return;
    }
    if (isAnalyzeMode_) {
        writeCommand("exit");
        isAnalyzeMode_ = false;
    } else {
        writeCommand("?");
    }
    setForceMode();
}

EngineEvent::Type WinboardAdapter::waitAfterMoveNowHandshake() {
    return isAnalyzeMode_ ? EngineEvent::Type::None : EngineEvent::Type::BestMove;
}

EngineEvent::Type WinboardAdapter::handlePonderMiss() {
    // XBoard engines don't send bestmove when stopping pondering
    // We just stop pondering silently, no handshake possible
    return EngineEvent::Type::None;
}

void WinboardAdapter::setPonder(bool enabled) {
	EngineAdapter::setPonder(enabled);
}

void WinboardAdapter::ticker() {
    // Currently unused in UCI
}

uint64_t WinboardAdapter::allowPonder(
    [[maybe_unused]] const GameStruct & game, 
    [[maybe_unused]] const GoLimits & limits, 
    [[maybe_unused]] std::string ponderMove) {
    return 0;
}

void WinboardAdapter::bestMoveReceived(const std::string& sanMove, const std::string& lanMove) {
    if (!sanMove.empty()) {
        gameStruct_.sanMoves += (gameStruct_.sanMoves.empty() ? "" : " ") + sanMove;
    }
    if (!lanMove.empty()) {
        gameStruct_.lanMoves += (gameStruct_.lanMoves.empty() ? "" : " ") + lanMove;
    }
    // The last own move is now in the move list
    lastOwnMove_.clear();
}

uint64_t WinboardAdapter::go(bool isInfinite) {
    if (isAnalyzeMode_) {
        return 0;
    }
    forceMode_ = false;
    if (isInfinite) {
        isAnalyzeMode_ = true;
        return writeCommand("analyze");
    }
    return writeCommand("go");
}

uint64_t WinboardAdapter::catchupMovesAndGo(const GameStruct& game, bool isInfinite) {

	const auto& newMoves = isEnabled("san") ? game.sanMoves : game.lanMoves;
	const auto& oldMoves = isEnabled("san") ? gameStruct_.sanMoves : gameStruct_.lanMoves;
    
    if (game.fen != gameStruct_.fen || !newMoves.starts_with(oldMoves)) {
        sendPosition(game);
        return go(isInfinite);
    }
    // If we have a last own move that was not reflected by "bestMoveReceived", the engine has made
    // an additional move (e.g. in force mode). We need to undo it first as the position is now out of sync.
    if (!lastOwnMove_.empty()) {
        setForceMode();
        writeCommand("undo");
        lastOwnMove_.clear();
    }
	std::string additionalMoves = newMoves.substr(oldMoves.size());

    if (additionalMoves.empty()) {
        return go(isInfinite);
    }

    std::istringstream lanStream(additionalMoves);
    std::string move;

    std::string firstMove;
    lanStream >> firstMove;
    uint64_t lastTimestamp = 0;

	while (lanStream >> move) {
        if (!firstMove.empty()) {
			// If we have more than one move, we need to set winboard to force mode to prevent the engine
			// from playing its own move immediately.
            setForceMode();
            writeCommand((isEnabled("usermove") ? "usermove " : "") + firstMove);
			firstMove.clear();
        }
        lastTimestamp = writeCommand((isEnabled("usermove") ? "usermove " : "") + move);
	}
    if (!firstMove.empty()) {
		lastTimestamp = writeCommand((isEnabled("usermove") ? "usermove " : "") + firstMove);
    }

    bool switchSides = lastTimestamp == 0;
    // Check for isInfinite is redundant as isInfinite sets forceMode_ to true, but clearer
    if (forceMode_ || switchSides || isInfinite) {
        lastTimestamp = go(isInfinite);
    }

    gameStruct_ = game;
    return lastTimestamp;
}

uint64_t WinboardAdapter::computeMove(const GameStruct& game,
    const GoLimits& limits,
    [[maybe_unused]] bool ponderHit) 
{
    if (limits.infinite) {
        setForceMode();
        return catchupMovesAndGo(game, true);
    } 
    if (isEnabled("time") && !limits.mateIn && !limits.depth && !limits.nodes && !limits.moveTimeMs) {
        uint64_t time = game.isWhiteToMove ? limits.wtimeMs : limits.btimeMs;
        uint64_t otim = game.isWhiteToMove ? limits.btimeMs : limits.wtimeMs;
        writeCommand("time " + std::to_string(time / 10));
        writeCommand("otim " + std::to_string(otim / 10));
    }
    return catchupMovesAndGo(game);
}

void WinboardAdapter::askForReady() {
    pingCounter_++;
    writeCommand("ping " + std::to_string(pingCounter_));
}

void WinboardAdapter::sendPosition(const GameStruct& game) {
    writeCommand("new");
    // The new command leaves force mode and sets white to move
    forceMode_ = false;
    writeCommand(ponderMode_ ? "hard" : "easy");
    setForceMode();

    if (!game.fen.empty()) {
        writeCommand("setboard " + game.fen);
    }

    std::istringstream lanStream(game.lanMoves);
    std::string move;

    while (lanStream >> move) {
        writeCommand((isEnabled("usermove") ? "usermove " : "") + move);
    }

}

void WinboardAdapter::setTestOption(
    [[maybe_unused]] const std::string& name, 
    [[maybe_unused]] const std::string& value) {
	throw AppError::make("WinboardAdapter does not support setTestOption");
}

std::string WinboardAdapter::computeStandardOptions(const EngineOption& supportedOption, const std::string& value) {
    // We use uci compatible option names for memory and smp
    std::string command;
    if (supportedOption.name == "Hash" && isEnabled("memory")) {
        command = "memory " + value;
    } else if (supportedOption.name == "Threads" && isEnabled("smp")) {
        command = "cores " + value;
    } else {
        // XBoard protocol: options are set with "option <name>=<value>"
        command = "option " + supportedOption.name + "=" + value;
    }
    return command;
}

void WinboardAdapter::setOptionValues(const OptionValues& optionValues) {
    for (const auto& [name, value] : optionValues) {
        try {
			auto opt = getSupportedOption(name);
            if (!opt) {
                Logger::testLogger().log(std::format("Unsupported option: {}", name), TraceLevel::info);
                continue;
            }
			const auto& supportedOption = *opt;
            // check type and  value constraints
            if (supportedOption.type == EngineOption::Type::String) {
                if (value.size() > 9999) {
                    Logger::testLogger().log(std::format("Option value for {} is too long", name), TraceLevel::info);
                    continue;
                }
            }
            else if (supportedOption.type == EngineOption::Type::Spin) {
                int intValue = std::stoi(value);
                if (intValue < supportedOption.min || intValue > supportedOption.max) {
                    Logger::testLogger().log(std::format("Option value for {} is out of bounds", name), TraceLevel::info);
                    continue;
                }
            }
            else if (supportedOption.type == EngineOption::Type::Check) {
                if (value != "true" && value != "false") {
                    Logger::testLogger().log(std::format("Invalid boolean value for option {}", name), TraceLevel::info);
                    continue;
                }
            }
            else if (supportedOption.type == EngineOption::Type::Combo) {
                if (std::ranges::find(supportedOption.vars, value) == supportedOption.vars.end()) {
                    Logger::testLogger().log(std::format("Invalid value for combo option {}", name), TraceLevel::info);
                    continue;
                }
            }
            std::string command = computeStandardOptions(supportedOption, value);
            writeCommand(command);
        }
        catch (...) {
            Logger::testLogger().log(std::format("Invalid value {} for option {}", value, name), TraceLevel::info);
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
static bool readBoundedInt(std::istringstream& iss,
    const std::string& fieldName,
    T min,
    T max,
    std::optional<T>& target,
    std::vector<EngineEvent::ParseError>& errors)
{
    int64_t value;
    if (!(iss >> value)) {
        errors.push_back({
            .name = "missing-thinking-output",
            .detail = "Expected an integer for '" + fieldName + "'"
            });
        return false;
    }

    if (value < static_cast<int64_t>(min) || value > static_cast<int64_t>(max)) {
        errors.push_back({
            fieldName,
            "Reported value " + std::to_string(value) +
            " is outside the expected range [" +
            std::to_string(min) + ", " + std::to_string(max) + "]"
            });
        target = 0;
        return true;
    }

    target = static_cast<T>(value);
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
    int64_t value = std::stol(token);

    if (value < static_cast<int64_t>(min) || value > static_cast<int64_t>(max)) {
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
static bool comesTab(std::istream& stream) {
    while (std::isspace(static_cast<unsigned char>(stream.peek())) != 0 && stream.peek() != '\t') {
        stream.get();
    }
    return stream.peek() == '\t';
}

static std::vector<std::string> parseOptionalIntegers(std::istringstream& iss, EngineEvent& event) {
	std::vector<std::string> pv;
    std::vector<std::string> optionals;
    std::streampos pvStart = iss.tellg();
    std::string token;
    while (iss >> token) {
		pv.push_back(token);
		if (comesTab(iss)) {
            optionals = std::move(pv);
            pv.clear();
			pvStart = iss.tellg();
		}
    }
	if (!optionals.empty()) {
		const auto& last = optionals.back();
		storeBoundedInt<uint64_t>(last, "tbhits", 0, std::numeric_limits<int64_t>::max(), event.searchInfo->tbhits, event.errors);
	}
    if (optionals.size() > 1) {
        const auto& selDepth = optionals[0];
		storeBoundedInt<uint32_t>(selDepth, "seldepth", 0, 1000, event.searchInfo->selDepth, event.errors);
    }
    if (optionals.size() > 2) {
        const auto& nps = optionals[1];
        storeBoundedInt<uint64_t>(nps, "nps", 0, std::numeric_limits<int64_t>::max(), event.searchInfo->nps, event.errors);
    }
	iss.seekg(pvStart);
    iss >> std::ws;
    std::string pvText;
    std::getline(iss, pvText);
	event.searchInfo->pvText = pvText;

    return pv;
}

static void parsePV(const std::vector<std::string>& pv, EngineEvent& event) {
    bool inParens = false;
    for (const auto& token : pv) {
        if (token.find('(') != std::string::npos) {
            inParens = true;
        }
        if (token.find(')') != std::string::npos) {
            inParens = false;
        }
        if (inParens) {
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(token[0])) != 0 || token == "0-0" || token == "0-0-0") {
			event.searchInfo->pv.push_back(token);
        }
    }
}

EngineEvent WinboardAdapter::parseSearchInfo(const std::string& depthStr, std::istringstream& iss, 
    uint64_t timestamp, const std::string& originalLine) {
    EngineEvent event = EngineEvent::createInfo(identifier_, timestamp, originalLine);
    constexpr int32_t MATE_VALUE = 100000;
    constexpr int32_t MAX_SCORE = MATE_VALUE + 10000;

    event.searchInfo->depth = std::stoi(depthStr);

	if (!readBoundedInt<int32_t>(iss, "score", -MAX_SCORE, MAX_SCORE, event.searchInfo->scoreCp, event.errors)) {
        return event;
	}
    if (*event.searchInfo->scoreCp <= -MATE_VALUE) {
        event.searchInfo->scoreMate = *event.searchInfo->scoreCp + MATE_VALUE;
    }
    else if (*event.searchInfo->scoreCp >= MATE_VALUE) {
        event.searchInfo->scoreMate = *event.searchInfo->scoreCp - MATE_VALUE;
    }

	if (!readBoundedInt<uint64_t>(iss, "time", 0, std::numeric_limits<int64_t>::max() / 10, event.searchInfo->timeMs, event.errors)) {
		return event;
	}
    *event.searchInfo->timeMs *= 10;

	if (!readBoundedInt<uint64_t>(iss, "nodes", 0, std::numeric_limits<int64_t>::max(), event.searchInfo->nodes, event.errors)) {
		return event;
	}

    // optionale ints (seldepth, nps, tbhits)
    auto pv = parseOptionalIntegers(iss, event);

    // principal variation
    parsePV(pv, event);

    return event;
}

void WinboardAdapter::parseOptionFeature(const std::string& optionStr, EngineEvent& event) {
    std::istringstream iss(optionStr);
    std::string name;
    std::getline(iss, name, '-');
    if (name.find(' ') != std::string::npos) {
        event.errors.push_back({ .name = "feature-report", .detail = "Option name '" + name + "' contains space" });
    }
    QaplaHelpers::trim(name);

    std::string kind;
    iss >> kind;

    EngineOption opt;
    opt.name = name;
    opt.type = EngineOption::parseType(kind);

    if (opt.type == EngineOption::Type::Spin || opt.type == EngineOption::Type::Slider) {
        std::string value;
        int min;
        int max;
        if (iss >> value >> min >> max) {
            opt.defaultValue = value;
            opt.min = min;
            opt.max = max;
        }
        else {
            event.errors.push_back({ .name = "feature-report", .detail = "Invalid spin/slider definition for '" + name + "'" });
            return;
        }
    }
    else if (opt.type == EngineOption::Type::Combo) {
        std::string token;
        while (iss >> token) {
            if (!token.empty() && token.front() == '*') {
                opt.defaultValue = token.substr(1);
                opt.vars.push_back(opt.defaultValue);
            }
            else {
                opt.vars.push_back(token);
            }
        }
    }
    else if (opt.type == EngineOption::Type::Check || opt.type == EngineOption::Type::String ||
        opt.type == EngineOption::Type::File || opt.type == EngineOption::Type::Path) {
        std::string value;
        std::getline(iss, value);
        QaplaHelpers::trim(value);
        opt.defaultValue = value;
    }

    supportedOptions_.push_back(std::move(opt));
}

EngineEvent WinboardAdapter::parseFeatureLine(std::istringstream& iss, uint64_t timestamp, bool onlyOption) {
    std::string token;
    EngineEvent event = EngineEvent::createNoData(identifier_, timestamp);

    while (iss >> token) {
        auto eqPos = token.find('=');
        std::string key = token.substr(0, eqPos);
        std::string value = (eqPos != std::string::npos) ? token.substr(eqPos + 1) : "";

        // feature supports quoted values (strings)
        if (!value.empty() && value.front() == '"') {
            std::string remainder;
            while (!value.ends_with("\"") && iss >> remainder) {
                value += " " + remainder;
            }
            if (value.ends_with("\"")) {
                value = value.substr(1, value.size() - 2);
            }
        }

        if (key == "option") {
            parseOptionFeature(value, event);
            continue;
        }

        if (onlyOption) {
			event.errors.push_back({ .name = "feature-report", .detail = "Unexpected feature '" + key + "' outside protocol initialization" });
            continue;
        }

        auto it = featureMap_.find(key);
        if (it != featureMap_.end() && key != "done") {
            event.errors.push_back({ .name = "feature-report", .detail = "Feature '" + key + "' specified more than once" });
        }
        featureMap_[key] = value;
    }
    finalizeFeatures();
    return event;
}

void WinboardAdapter::finalizeFeatures() {
    static const std::unordered_map<std::string, bool> booleanFeatureDefaults = {
        { "ping", false },
        { "setboard", false },
        { "playother", false },
        { "san", false },
        { "usermove", false },
        { "time", true },
        { "draw", true },
        { "sigint", true },
        { "sigterm", true },
        { "reuse", true },
        { "analyze", true },
        { "colors", true },
        { "ics", false },
        { "name", false },
        { "pause", false },
        { "nps", true },
        { "debug", false },
        { "memory", false },
        { "smp", false },
        { "exclude", false },
        { "setscore", false },
        { "highlight", false }
    };

    for (const auto& [key, defaultValue] : booleanFeatureDefaults) {
        if (!featureMap_.contains(key)) {
            featureMap_[key] = defaultValue ? "1" : "0";
        }
    }
    if (featureMap_.contains("myname")) {
        engineName_ = featureMap_["myname"];
    }

    // XBoard protocol: if feature memory=1, add a Hash option for UCI compatibility (once)
    if (isEnabled("memory") && !getSupportedOption("Hash")) {
        EngineOption hashOption = {
            .name = "Hash",
            .type = EngineOption::Type::Spin,
            .defaultValue = "32",
            .min = 1,
            .max = 131072,
            .vars = {}
        };
        supportedOptions_.push_back(std::move(hashOption));
    }

    // XBoard protocol: if feature smp=1, add a Threads option
    if (isEnabled("smp") && !getSupportedOption("Threads")) {
        EngineOption threadsOption = {
            .name = "Threads",
            .type = EngineOption::Type::Spin,
            .defaultValue = "1",
            .min = 1,
            .max = 512,
            .vars = {}
        };
        supportedOptions_.push_back(std::move(threadsOption));
    }
}

EngineEvent WinboardAdapter::readFeatureSection(const EngineLine& engineLine) {
    const std::string& line = QaplaHelpers::trim(engineLine.content);

    if (!line.starts_with("feature ")) {
        logFromEngine(line, TraceLevel::info);
        return EngineEvent::createUnknown(identifier_, engineLine.timestampMs, line);
    }

    logFromEngine(line, TraceLevel::command);

    std::istringstream iss(line.substr(8)); // nach "feature "
    auto event = parseFeatureLine(iss, engineLine.timestampMs, false);

    auto it = featureMap_.find("done");
    if (it != featureMap_.end()) {
        event.rawLine = line;
        if (it->second == "1") {
            inFeatureSection_ = false;
            event.type = EngineEvent::Type::ProtocolOk;
        }
        else if (it->second == "0") {
            event.type = EngineEvent::Type::ExtendTimeout;
        }
        else {
			event.errors.push_back({ .name = "feature-report", .detail = "Invalid 'done' value: '" + it->second + "'" });
            event.type = EngineEvent::Type::Error;
        }
    }

    return event;
}

EngineEvent WinboardAdapter::parseResult(std::istringstream& iss, const std::string& command, EngineEvent event) {
    
    iss >> std::ws;
    char openBrace;
    if (!(iss >> openBrace) || openBrace != '{') {
        event.errors.push_back({
            .name = "result-parsing",
            .detail = "Expected opening '{' after game result command, in line: " + event.rawLine
			});
    }
    std::string text;
    if (!std::getline(iss, text, '}')) {
        event.errors.push_back({
            .name = "result-parsing",
            .detail = "Expected closing '}' at the end of a result command in line: " + event.rawLine
            });
    }
    text = QaplaHelpers::trim(text);

    if (command == "0-1") {
        event.gameResult = GameResult::BlackWins;
        if (text != "Black mates") {
            event.errors.push_back({
                .name = "result-parsing",
                .detail = "Expected 'Black mates' after '0-1' in: " + event.rawLine
            });
		}
    }
    else if (command == "1-0") {
        event.gameResult = GameResult::WhiteWins;
        if (text != "White mates") {
            event.errors.push_back({
                .name = "result-parsing",
                .detail = "Expected 'White mates' after '1-0' in: " + event.rawLine
                });
        }
    }
    else if (command == "1/2-1/2") {
        event.gameResult = GameResult::Draw;
    }
    else {
        event.errors.push_back({
            .name = "result-parsing",
            .detail = "Unexpected game result command: " + command + " in line: " + event.rawLine
			});
	}
    return event;
}

EngineEvent WinboardAdapter::parseCommentLine(const EngineLine& engineLine) {
    // XBoard protocol: engines with "feature debug=1" may send lines starting with '#' for debugging
    // These lines should be ignored when debug mode is enabled, but trigger an error otherwise
    if (isEnabled("debug")) {
        logFromEngine(engineLine.content, TraceLevel::info);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }
    logFromEngine(engineLine.content, TraceLevel::error);
    return EngineEvent::createError(identifier_, engineLine.timestampMs, 
        "Engine sent debug output without debug mode enabled");
}

EngineEvent WinboardAdapter::parseMove(std::istringstream& iss, const EngineLine& engineLine) {
    logFromEngine(engineLine.content, TraceLevel::command);
    std::string move;
    iss >> move;
    gameStruct_.originalMove = move;
    // lastOwnMove is used to remember that the engine is one move ahead until bestMoveReceived is called
    // reflecting that the move is now known to the game management
    lastOwnMove_ = move;
    return EngineEvent::createBestMove(identifier_, engineLine.timestampMs, engineLine.content, move, "");
}

EngineEvent WinboardAdapter::parseHint(std::istringstream& iss, const EngineLine& engineLine) {
    logFromEngine(engineLine.content, TraceLevel::command);
    std::string hintMove;
    iss >> hintMove;
    if (!hintMove.empty()) {
        return EngineEvent::createPonderMove(identifier_, engineLine.timestampMs, engineLine.content, hintMove);
    }
    return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
}

EngineEvent WinboardAdapter::parseCommand(const EngineLine& engineLine) {

    const auto& line = engineLine.content;
    std::istringstream iss(line);
    std::string command;
    iss >> command;
    command = QaplaHelpers::to_lowercase(command);

    if (QaplaHelpers::isInteger(command)) {
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
        return parseMove(iss, engineLine);
    }

    if (command == "tellics" || command == "tellicsnoalias" 
        || command == "tellusererror" || command == "tellallerror") 
    {
        logFromEngine(line, TraceLevel::info);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (command == "hint:") {
        return parseHint(iss, engineLine);
    }

    if (command == "feature") {
        logFromEngine(line, TraceLevel::command);
        return parseFeatureLine(iss, engineLine.timestampMs, true);
    }

    if (command == "resign") {
        logFromEngine(line, TraceLevel::command);
        return EngineEvent::create(EngineEvent::Type::Resign, identifier_, engineLine.timestampMs, line);
    }

    if (command == "offer") {
        logFromEngine(line, TraceLevel::command);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (command == "0-1" || command == "1-0" || command == "1/2-1/2") {
        logFromEngine(line, TraceLevel::command);
        EngineEvent event = EngineEvent::create(
            EngineEvent::Type::Result, identifier_, engineLine.timestampMs, line);
		return parseResult(iss, command, std::move(event));
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

EngineEvent WinboardAdapter::readEvent() {
    EngineLine engineLine = process_.readLineBlocking();
    const auto& line = engineLine.content;

    if (!engineLine.complete) {
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (engineLine.error == EngineLine::Error::IncompleteLine) {
        logFromEngine(line, TraceLevel::error);
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }

    if (engineLine.error == EngineLine::Error::EngineTerminated && terminating_) {
        return EngineEvent::createNoData(identifier_, engineLine.timestampMs);
    }
    if (engineLine.error == EngineLine::Error::EngineTerminated && !terminating_) {
        return EngineEvent::createEngineDisconnected(identifier_, engineLine.timestampMs, engineLine.content);
    }

    if (inFeatureSection_) {
        return readFeatureSection(engineLine);
    }

    if (!line.empty() && line[0] == '#') {
        return parseCommentLine(engineLine);
    }

    return parseCommand(engineLine);
   
}

} // namespace QaplaTester
