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

#include "perft.h"

#include <algorithm>
#include <atomic>
#include <thread>

namespace QaplaTester {

uint64_t Perft::countNodes(GameState& state, uint32_t depth) {
    if (depth == 0) {
        return 1;
    }
    const auto moves = state.getLegalMoves();
    if (depth == 1) {
        return moves.size();
    }
    uint64_t nodes = 0;
    for (const auto& move : moves) {
        state.doMove(move);
        nodes += countNodes(state, depth - 1);
        state.undoMove();
    }
    return nodes;
}

PerftResult Perft::run(const GameState& startState, uint32_t depth, uint32_t concurrency,
    bool divide, bool showFen) {
    PerftResult result;
    if (depth == 0) {
        result.nodes = 1;
        return result;
    }

    GameState rootState = startState;
    const auto rootMoves = rootState.getLegalMoves();
    result.divide.resize(rootMoves.size());

    const uint32_t threadCount = std::max<uint32_t>(1,
        std::min<uint32_t>(concurrency, static_cast<uint32_t>(rootMoves.size())));

    std::atomic<size_t> nextIndex{ 0 };

    const auto worker = [&]() {
        GameState localState = rootState;
        for (size_t index = nextIndex.fetch_add(1); index < rootMoves.size(); index = nextIndex.fetch_add(1)) {
            const auto& move = rootMoves[index];
            localState.doMove(move);
            auto& entry = result.divide[index];
            entry.move = move.getLAN();
            entry.nodes = countNodes(localState, depth - 1);
            if (divide && showFen) {
                entry.fenAfter = localState.getFen();
            }
            localState.undoMove();
        }
    };

    if (threadCount <= 1) {
        worker();
    } else {
        std::vector<std::thread> workers;
        workers.reserve(threadCount);
        for (uint32_t i = 0; i < threadCount; ++i) {
            workers.emplace_back(worker);
        }
        for (auto& workerThread : workers) {
            workerThread.join();
        }
    }

    for (const auto& entry : result.divide) {
        result.nodes += entry.nodes;
    }

    if (!divide) {
        result.divide.clear();
    }

    return result;
}

} // namespace QaplaTester
