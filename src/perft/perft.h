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

#include "../game-manager/game-state.h"

#include <cstdint>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief Node count for a single root move of a perft divide run.
 */
struct PerftMoveResult {
    std::string move;      ///< The root move in LAN notation.
    uint64_t nodes = 0;     ///< Number of leaf nodes reached below this move.
    std::string fenAfter;   ///< FEN after the move, only set if requested.
};

/**
 * @brief Aggregated result of a perft run.
 */
struct PerftResult {
    uint64_t nodes = 0;                     ///< Total number of leaf nodes at the requested depth.
    std::vector<PerftMoveResult> divide;    ///< Per-root-move breakdown, empty unless divide was requested.
};

/**
 * @brief Computes perft (move path enumeration) node counts using GameState's move generation,
 *        doMove and undoMove. Contains no chess logic of its own.
 */
class Perft {
public:
    /**
     * @brief Runs perft from the given position.
     *
     * Root moves are distributed across up to 'concurrency' worker threads, one root move
     * being the smallest unit of work. If there are fewer legal root moves than 'concurrency',
     * only as many threads as there are root moves are started.
     *
     * @param startState Position to search from.
     * @param depth Search depth in plies.
     * @param concurrency Maximum number of worker threads used to split root moves.
     * @param divide If true, the per-root-move breakdown is kept in the result.
     * @param showFen If true (and divide is true), the FEN after each root move is recorded.
     * @return The aggregated perft result.
     */
    [[nodiscard]] static PerftResult run(const GameState& startState, uint32_t depth,
        uint32_t concurrency, bool divide, bool showFen);

private:
    /**
     * @brief Recursively counts the leaf nodes below the current position.
     * @param state Position to search from, modified in place via doMove/undoMove.
     * @param depth Remaining search depth in plies.
     * @return Number of leaf nodes.
     */
    [[nodiscard]] static uint64_t countNodes(GameState& state, uint32_t depth);
};

} // namespace QaplaTester
