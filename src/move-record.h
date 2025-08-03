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
#include <assert.h>
#include <sstream>
#include <iomanip>
#include "engine-event.h"

struct MoveRecord {
    std::string original{};
    std::string lan{};
    std::string san{};
    std::string comment{};
    std::string nag{};
    uint64_t timeMs = 0;

    std::optional<int> scoreCp = std::nullopt;
    std::optional<int> scoreMate = std::nullopt;

    uint32_t halfmoveClock = 0; 
    uint32_t depth = 0;
    uint32_t seldepth = 0;
    uint32_t multipv = 1;
    uint64_t nodes = 0;
    std::string pv{};

    void clear() {
		original.clear();
		lan.clear();
        san.clear();
		comment.clear();
		nag.clear();
		timeMs = 0;
		scoreCp.reset();
		scoreMate.reset();
		depth = 0;
		seldepth = 0;
		multipv = 1;
		nodes = 0;
		pv.clear();
    }

    /**
     * Updates the move record with the best move and time taken from an EngineEvent.
     *
     * @param event EngineEvent of type BestMove containing the chosen move and timestamp.
     * @param lanMove move in long algebraic notation
     * @param sanMove move in short algebraic notation
     * @param computeStartTimestamp Timestamp when move computation started, in milliseconds.
     * @param halfmoveClk The current halfmove clock value, used for the 50-move rule.
     */
    void updateFromBestMove(const EngineEvent& event, std::string lanMove, std::string sanMove,
        uint64_t computeStartTimestamp, uint32_t halfmoveClk) {
        if (event.bestMove) {
            original = *event.bestMove;
			lan = lanMove;
            san = sanMove;
        }
        halfmoveClock = halfmoveClk;
        timeMs = event.timestampMs - computeStartTimestamp;
    }

    /**
	 * @brief Updates the move record with search information from an EngineEvent.
	 * @param info SearchInfo containing various search metrics.
     */
    void updateFromSearchInfo(const SearchInfo& info) {
        if (info.depth) depth = *info.depth;
        if (info.selDepth) seldepth = *info.selDepth;
        if (info.multipv) multipv = *info.multipv;
        if (info.nodes) nodes = static_cast<uint64_t>(*info.nodes);

        if (info.scoreCp) {
            scoreCp = *info.scoreCp;
            scoreMate.reset();
		}
		else if (info.scoreMate) {
			scoreMate = *info.scoreMate;
			scoreCp.reset();
		}

        if (!info.pv.empty()) {
            pv.clear();
            for (size_t i = 0; i < info.pv.size(); ++i) {
                if (i > 0) pv += ' ';
                pv += info.pv[i];
            }
        }
    }

	/**
	 * @brief Returns a string representation of the score.
	 *
	 * @return A string representing the score in centipawns or mate value.
	 */
    std::string evalString() const {
		assert(!(scoreCp && scoreMate));
        if (scoreMate) {
            return *scoreMate > 0 ? "M" + std::to_string(*scoreMate) : "-M" + std::to_string(-*scoreMate);
        }
        if (scoreCp) {
            std::ostringstream oss;
            oss << (*scoreCp >= 0 ? "+" : "")
                << std::fixed << std::setprecision(2) << (*scoreCp / 100.0);
            return oss.str();
        }
        return "?";
    }
};
