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
#include "engine-adapter.h"


struct MoveRecord {
    std::string san;
    std::string comment;
    std::string nag;
    uint64_t timeMs = 0;

    std::optional<int> scoreCp;
    std::optional<int> scoreMate;

    int depth = 0;
    int seldepth = 0;
    int multipv = 1;
    uint64_t nodes = 0;
    std::string pv;



    /**
     * Updates the move record with the best move and time taken from an EngineEvent.
     *
     * @param event EngineEvent of type BestMove containing the chosen move and timestamp.
     * @param computeStartTimestamp Timestamp when move computation started, in milliseconds.
     */
    void updateFromBestMove(const EngineEvent& event, int64_t computeStartTimestamp) {
        if (event.bestMove) {
            san = *event.bestMove;
        }
        timeMs = static_cast<uint64_t>(event.timestampMs - computeStartTimestamp);
    }

    /**
	 * @brief Updates the move record with search information from an EngineEvent.
	 * @param event EngineEvent of type Info containing search information.
     */
    void updateFromSearchInfo(const SearchInfo& info) {
        if (info.depth) depth = *info.depth;
        if (info.selDepth) seldepth = *info.selDepth;
        if (info.multipv) multipv = *info.multipv;
        if (info.nodes) nodes = static_cast<uint64_t>(*info.nodes);

        if (info.scoreCp) scoreCp = *info.scoreCp;
        if (info.scoreMate) scoreMate = *info.scoreMate;

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
        if (scoreMate) {
            return *scoreMate > 0 ? "M" + std::to_string(*scoreMate) : "-M" + std::to_string(-*scoreMate);
        }
        if (scoreCp) {
            return (*scoreCp >= 0 ? "+" : "") + std::to_string(*scoreCp);
        }
        return "?";
    }
};
