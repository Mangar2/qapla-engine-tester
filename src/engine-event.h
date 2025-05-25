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
#pragma once

#include <string>
#include <optional>
#include <ostream>

struct SearchInfo {
    std::optional<int> depth;
    std::optional<int> selDepth;
    std::optional<int> multipv;
    std::optional<int> scoreCp;
    std::optional<int> scoreMate;
    std::optional<bool> scoreLowerbound;
    std::optional<bool> scoreUpperbound;
    std::optional<int64_t> timeMs;
    std::optional<int64_t> nodes;
    std::optional<int64_t> nps;
    std::optional<int> hashFull;
    std::optional<int> tbhits;
    std::optional<int> cpuload;
    std::optional<int> currMoveNumber;
    std::optional<std::string> currMove;
    std::optional<int> refutationIndex;
    std::vector<std::string> pv;
    std::vector<std::string> errors;  // für Parsing-Fehler oder unklare Angaben
};

inline std::ostream& operator<<(std::ostream& os, const SearchInfo& info) {
    os << "info";

    if (info.depth)          os << " depth " << *info.depth;
    if (info.selDepth)       os << " seldepth " << *info.selDepth;
    if (info.multipv)        os << " multipv " << *info.multipv;

    if (info.scoreCp || info.scoreMate) {
        os << " score";
        if (info.scoreCp)    os << " cp " << *info.scoreCp;
        else if (info.scoreMate) os << " mate " << *info.scoreMate;

        if (info.scoreLowerbound && *info.scoreLowerbound) os << " lowerbound";
        if (info.scoreUpperbound && *info.scoreUpperbound) os << " upperbound";
    }

    if (info.timeMs)         os << " time " << *info.timeMs;
    if (info.nodes)          os << " nodes " << *info.nodes;
    if (info.nps)            os << " nps " << *info.nps;
    if (info.hashFull)       os << " hashfull " << *info.hashFull;
    if (info.tbhits)         os << " tbhits " << *info.tbhits;
    if (info.cpuload)        os << " cpuload " << *info.cpuload;
    if (info.currMove)       os << " currmove " << *info.currMove;
    if (info.currMoveNumber) os << " currmovenumber " << *info.currMoveNumber;

    if (!info.pv.empty()) {
        os << " pv";
        for (const auto& move : info.pv) {
            os << " " << move;
        }
    }

    return os;
}

struct EngineEvent {
    EngineEvent() = default;
    enum class Type {
        None,
        ComputeMoveSent,
        ReadyOk,
        UciOk,
        BestMove,
        Info,
        PonderHit,
        Error,
        EngineDisconnected,
        Unknown,
        NoData,
        KeepAlive,
    };
    static EngineEvent create(Type type, const std::string& id, int64_t ts, const std::string& rawLine = "") {
        EngineEvent e; 
        e.type = type;
        e.engineIdentifier = id;
        e.timestampMs = ts;
        e.rawLine = rawLine;
        return e;
    }
	static EngineEvent createEngineDisconnected(const std::string& id, int64_t ts, const std::string& errorMessage) {
        EngineEvent e = create(Type::EngineDisconnected, id, ts, "");
		e.errors.push_back({ "I/O Error", errorMessage });
		return e;
	}
	static EngineEvent createNoData(const std::string& id, int64_t ts) {
		return create(Type::NoData, id, ts);
	}
	static EngineEvent createUciOk(const std::string& id, int64_t ts, const std::string& rawLine) {
		return create(Type::UciOk, id, ts, rawLine);
	}
	static EngineEvent createReadyOk(const std::string& id, int64_t ts, const std::string& rawLine) {
		return create(Type::ReadyOk, id, ts, rawLine);
	}
    static EngineEvent createPonderHit(const std::string& id, int64_t ts, const std::string& rawLine) {
		return create(Type::PonderHit, id, ts, rawLine);
	}
	static EngineEvent createUnknown(const std::string& id, int64_t ts, const std::string& rawLine) {
		return create(Type::Unknown, id, ts, rawLine);
	}
	static EngineEvent createBestMove(const std::string& id, int64_t ts, const std::string& rawLine, 
        const std::string& bestMove, const std::string& ponderMove) {
		EngineEvent e = create(Type::BestMove, id, ts, rawLine);
		e.bestMove = bestMove;
		e.ponderMove = ponderMove;
		return e;
	}

    struct ParseError {
        std::string name;
        std::string detail;
    };

    Type type;
    int64_t timestampMs;
    std::string rawLine;

    std::optional<std::string> bestMove;
    std::optional<std::string> ponderMove;

    std::optional<SearchInfo> searchInfo;
    std::vector<ParseError> errors;
    std::string engineIdentifier;
private:

};

