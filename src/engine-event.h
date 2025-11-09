/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSevent.  See the
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

#include "game-result.h"
#include "logger.h"

namespace QaplaTester {

struct SearchInfo {
    uint32_t halfMoveNo = 0;
    std::optional<uint32_t> depth;
    std::optional<uint32_t> selDepth;
    std::optional<uint32_t> multipv;
    std::optional<int> scoreCp;
    std::optional<int32_t> scoreMate;
    std::optional<bool> scoreLowerbound;
    std::optional<bool> scoreUpperbound;
    std::optional<uint64_t> timeMs;
    std::optional<uint64_t> nodes;
    std::optional<uint64_t> nps;
    std::optional<uint32_t> hashFull;
    std::optional<uint64_t> tbhits;
    std::optional<uint32_t> sbhits;
    std::optional<uint32_t> cpuload;
    std::optional<uint32_t> currMoveNumber;
    std::optional<std::string> currMove;
    std::optional<uint32_t> refutationIndex;
    std::optional<std::string> pvText;
    std::vector<std::string> pv;
	std::vector<std::string> refutation;
    std::vector<std::string> currline; 
    std::vector<std::string> errors;  // für Parsing-Fehler oder unklare Angaben
};

inline std::ostream& operator<<(std::ostream& outs, const SearchInfo& info) {
    outs << "info";

    if (info.depth) {
        outs << " depth " << *info.depth;
    }
    if (info.selDepth) {
        outs << " seldepth " << *info.selDepth;
    }
    if (info.multipv) {
        outs << " multipv " << *info.multipv;
    }

    if (info.scoreCp || info.scoreMate) {
        outs << " score";
        if (info.scoreCp) {
            outs << " cp " << *info.scoreCp;
        } else if (info.scoreMate) {
            outs << " mate " << *info.scoreMate;
        }

        if (info.scoreLowerbound && *info.scoreLowerbound) { outs << " lowerbound";
            outs << " lowerbound";
        }
        if (info.scoreUpperbound && *info.scoreUpperbound) {
            outs << " upperbound";
        }
    }

    if (info.timeMs) {
        outs << " time " << *info.timeMs;
    }
    if (info.nodes) {
        outs << " nodes " << *info.nodes;
    }
    if (info.nps) {
        outs << " nps " << *info.nps;
    }
    if (info.hashFull) {
        outs << " hashfull " << *info.hashFull;
    }
    if (info.tbhits) {
        outs << " tbhits " << *info.tbhits;
    }
    if (info.cpuload) {
        outs << " cpuload " << *info.cpuload;
    }
    if (info.currMove) {
        outs << " currmove " << *info.currMove;
    }
    if (info.currMoveNumber) {
        outs << " currmovenumber " << *info.currMoveNumber;
    }

    if (!info.pv.empty()) {
        outs << " pv";
        for (const auto& move : info.pv) {
            outs << " " << move;
        }
    }

    return outs;
}

struct EngineEvent {
    enum class Type : std::int8_t {
        None,
        SendingComputeMove,
        ComputeMoveSent,
        PonderMoveSent,
        ReadyOk,
        ProtocolOk,
        ExtendTimeout,
        BestMove,
        PonderMove,
        Info,
        PonderHit,
        Resign,
        Result,
        Error,
        EngineDisconnected,
        Unknown,
        NoData,
        KeepAlive,
    };
    static EngineEvent create(Type type, const std::string& eid, uint64_t timestamp, const std::string& rawLine = "") {
        EngineEvent event;
        event.type = type;
        event.engineIdentifier = eid;
        event.timestampMs = timestamp;
        event.rawLine = rawLine;
        return event;
    }
    static EngineEvent createInfo(const std::string& eid, uint64_t timestamp, const std::string& rawLine) {
        auto event = create(Type::Info, eid, timestamp, rawLine);
        event.searchInfo = SearchInfo{};
        return event;
	}
    static EngineEvent createError(const std::string& eid, uint64_t timestamp, const std::string& rawLine) {
        EngineEvent event = create(Type::Error, eid, timestamp, rawLine);
        event.errors.push_back({ .name = "no-engine-error-report", .detail = rawLine });
        return event;
	}
	static EngineEvent createEngineDisconnected(const std::string& eid, uint64_t timestamp, const std::string& errorMessage) {
        EngineEvent event = create(Type::EngineDisconnected, eid, timestamp, "");
		event.errors.push_back({ .name = "no-disconnect", .detail = errorMessage });
		return event;
	}
	static EngineEvent createNoData(const std::string& eid, uint64_t timestamp) {
		return create(Type::NoData, eid, timestamp);
	}
	static EngineEvent createProtocolOk(const std::string& eid, uint64_t timestamp, const std::string& rawLine) {
		return create(Type::ProtocolOk, eid, timestamp, rawLine);
	}
	static EngineEvent createReadyOk(const std::string& eid, uint64_t timestamp, const std::string& rawLine) {
		return create(Type::ReadyOk, eid, timestamp, rawLine);
	}
    static EngineEvent createPonderHit(const std::string& eid, uint64_t timestamp, const std::string& rawLine) {
		return create(Type::PonderHit, eid, timestamp, rawLine);
	}
	static EngineEvent createUnknown(const std::string& eid, uint64_t timestamp, const std::string& rawLine) {
		return create(Type::Unknown, eid, timestamp, rawLine);
	}
	static EngineEvent createBestMove(const std::string& eid, uint64_t timestamp, const std::string& rawLine, 
        const std::string& bestMove, const std::string& ponderMove) {
		EngineEvent event = create(Type::BestMove, eid, timestamp, rawLine);
		event.bestMove = bestMove;
		event.ponderMove = ponderMove;
		return event;
	}
    static EngineEvent createPonderMove(const std::string&eid, uint64_t timestamp, const std::string& rawLine,
        const std::string& ponderMove) {
        EngineEvent event = create(Type::PonderMove, eid, timestamp, rawLine);
        event.ponderMove = ponderMove;
        return event;
    }

    struct ParseError {
        std::string name;
        std::string detail;
        TraceLevel level = TraceLevel::info;
    };

    Type type;
    uint64_t timestampMs;
    bool computing = false; // Indicates if the event is related to the engine computing a move
    std::string rawLine;

    std::optional<std::string> bestMove;
    std::optional<std::string> ponderMove;

    std::optional<SearchInfo> searchInfo;
    std::vector<ParseError> errors;
    std::string engineIdentifier;
	std::optional<std::string> stringInfo;
    std::optional<GameResult> gameResult;
private:

};

} // namespace QaplaTester
