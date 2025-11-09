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

#include "adjudication-manager.h"
#include "string-helper.h"

namespace QaplaTester {

std::pair<GameEndCause, GameResult> AdjudicationManager::adjudicateDraw(const GameRecord& game) const {
    if (!drawConfig_.active || drawConfig_.testOnly) {
		return { GameEndCause::Ongoing, GameResult::Unterminated };
    }
    const auto& moves = game.history();
    const auto& cfg = drawConfig_;

    if (cfg.requiredConsecutiveMoves <= 0 
        || cfg.centipawnThreshold <= 0 
        || moves.size() < 2 * cfg.minFullMoves ) {
        return { GameEndCause::Ongoing, GameResult::Unterminated };
    }

    if (moves.empty() || moves.back().halfmoveClock < 2 * cfg.requiredConsecutiveMoves) {
        return { GameEndCause::Ongoing, GameResult::Unterminated };
    }

    uint32_t consecutiveOk = 0;

    for (int32_t i = static_cast<int32_t>(moves.size()) - 1; i >= 0; --i) {
        const auto& move = moves[static_cast<uint32_t>(i)];

        if (!move.scoreCp || std::abs(*move.scoreCp) > cfg.centipawnThreshold) {
            break;
        }

        consecutiveOk++;
        if (consecutiveOk >= 2 * cfg.requiredConsecutiveMoves) {
            return { GameEndCause::Adjudication, GameResult::Draw };
        }
    }

    return { GameEndCause::Ongoing, GameResult::Unterminated };
}

 std::pair<GameEndCause, GameResult> AdjudicationManager::adjudicateResign(const GameRecord& game) const {
    if (!resignConfig_.active || resignConfig_.testOnly) {
        return { GameEndCause::Ongoing, GameResult::Unterminated };
    }
    const auto& moves = game.history();
    const auto& cfg = resignConfig_;

    if (cfg.requiredConsecutiveMoves <= 0 
        || cfg.centipawnThreshold <= 0 
        || moves.size() < static_cast<size_t>(2 * cfg.requiredConsecutiveMoves)) {
        return { GameEndCause::Ongoing, GameResult::Unterminated };
    }

    uint32_t consecutive = 0;

	const auto& lastMove = moves.back();
    bool curLoosing = lastMove.scoreCp && *lastMove.scoreCp <= -cfg.centipawnThreshold;
    // If white to move, the last move is a black move. When last last move looses the opponent wins
    GameResult prospectiveResult = game.isWhiteToMove() == curLoosing ? GameResult::WhiteWins : GameResult::BlackWins;

    for (int i = static_cast<int>(moves.size()) - 1; i >= 0; --i) {
        const auto& move = moves[static_cast<uint32_t>(i)];

        if (curLoosing) {
            if (!move.scoreCp || *move.scoreCp > -cfg.centipawnThreshold) {
                break;
            }
        } else if (cfg.twoSided) {
            if (!move.scoreCp || *move.scoreCp < cfg.centipawnThreshold) {
                break;
            }
        }
        consecutive++;

        if (consecutive >= 2 * cfg.requiredConsecutiveMoves) {
            return { GameEndCause::Adjudication, prospectiveResult };
        }
        curLoosing = !curLoosing;
    }

    return { GameEndCause::Ongoing, GameResult::Unterminated };
}

/**
 * @brief Determines who was to move at the start of the game.
 * @param game The game record.
 * @return True if white was to move at ply 0, false if black.
 */
 static bool wtmAtPly(const GameRecord& game, size_t ply) {
    if (ply > game.history().size()) {
        throw std::out_of_range("Ply index exceeds game history size");
    }
    return ((game.history().size() - ply) % 2 == 0) ? game.isWhiteToMove() : !game.isWhiteToMove();
}

std::optional<size_t> AdjudicationManager::findDrawAdjudicationIndex(const GameRecord& game) const {
	const auto& cfg = drawConfig_;
    const auto& moves = game.history();
    if (cfg.requiredConsecutiveMoves <= 0 || cfg.centipawnThreshold <= 0 || moves.size() < 2 * cfg.minFullMoves) {
        return std::nullopt;
    }

    uint32_t inRangeCount = 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        const auto& move = moves[i];

        if (!move.scoreCp || std::abs(*move.scoreCp) > cfg.centipawnThreshold) {
            inRangeCount = 0;
        } else {
            inRangeCount++;
            if (inRangeCount >= 2 * cfg.requiredConsecutiveMoves &&
                i + 1 >= 2 * static_cast<size_t>(cfg.minFullMoves) &&
                move.halfmoveClock >= 2 * cfg.requiredConsecutiveMoves) {
                return i;
            }
        }
    }

    return std::nullopt;
}

static uint32_t updateConsecutiveCounts(bool loosing, uint32_t consecutive) {
    return loosing ? consecutive + 1 : 0;
}

static bool isValidResignConfig(const AdjudicationManager::ResignAdjudicationConfig& config, 
    const std::vector<MoveRecord>& moves) {
    return config.requiredConsecutiveMoves > 0 
        && config.centipawnThreshold > 0 
        && moves.size() >= 2 * static_cast<size_t>(config.requiredConsecutiveMoves);
}

std::pair<GameResult, size_t> AdjudicationManager::findResignAdjudicationIndex(const GameRecord& game) const {
    
    const auto& moves = game.history();
    if (!isValidResignConfig(resignConfig_, moves)) {
        return { GameResult::Unterminated , 0 };
    }

     // We must determine who moves first, since the game may start from a FEN with black to move
    bool wtm = wtmAtPly(game, 0);

    // Separate counters are required to independently track each side’s uninterrupted losing streak
    uint32_t wConsecutive = 0;
    uint32_t bConsecutive = 0;
    // Required to ensure that both sides satisfy their respective conditions when two-sided resign is active
    uint32_t requiredConsecutive = resignConfig_.twoSided ? 
        2 * resignConfig_.requiredConsecutiveMoves : resignConfig_.requiredConsecutiveMoves;

    for (size_t i = 0; i < moves.size(); ++i) {
        const auto& move = moves[i];

        bool loosing = move.scoreCp && *move.scoreCp <= -resignConfig_.centipawnThreshold;
        if (wtm) {
            wConsecutive = updateConsecutiveCounts(loosing, wConsecutive);
        } else {
            bConsecutive = updateConsecutiveCounts(loosing, bConsecutive);
        }

        if (resignConfig_.twoSided) {
            bool winning = move.scoreCp && *move.scoreCp >= resignConfig_.centipawnThreshold;
            if (wtm) {
                bConsecutive = updateConsecutiveCounts(winning, bConsecutive);
            } else {
                wConsecutive = updateConsecutiveCounts(winning, wConsecutive);
            }
        }

        if (wConsecutive >= requiredConsecutive) {
            return { GameResult::BlackWins, i };
        }
        if (bConsecutive >= requiredConsecutive) {
            return { GameResult::WhiteWins, i };
        }

        wtm = !wtm;
    }

    return { GameResult::Unterminated, 0 };
}

void AdjudicationManager::onGameFinished(const GameRecord& game) {
    const auto& [finalCause, finalResult] = game.getGameResult();
    const auto& moves = game.history();

    if (drawConfig_.active && drawConfig_.testOnly) {
        drawStats_.totalGames++;

        if (auto index = findDrawAdjudicationIndex(game)) {
            for (size_t i = *index + 1; i < moves.size(); ++i) {
                drawStats_.savedTimeMs += moves[i].timeMs;
            }

            if (finalResult == GameResult::Draw) {
                drawStats_.correctDecisions++;
            } else {
                drawStats_.incorrectDecisions++;
                drawStats_.failed.push_back(game);
            }
        }

        for (const auto& m : moves) {
            drawStats_.totalTimeMs += m.timeMs;
        }
    }

    if (resignConfig_.active && resignConfig_.testOnly) {
        resignStats_.totalGames++;
        auto [adjudicatedResult, index] = findResignAdjudicationIndex(game);
        if (adjudicatedResult != GameResult::Unterminated) {
            for (size_t i = index + 1; i < moves.size(); ++i) {
                resignStats_.savedTimeMs += moves[i].timeMs;
            }

            if (finalResult == adjudicatedResult) {
                resignStats_.correctDecisions++;
            } else {
                resignStats_.incorrectDecisions++;
                resignStats_.failed.push_back(game);
            }
        }
        auto [wtime, btime] = game.timeUsed();
        resignStats_.totalTimeMs += wtime + btime;
    }
}

AdjudicationManager::TestResults AdjudicationManager::computeTestResults() const {
    TestResults results;
    
    results.hasDrawTest = drawConfig_.active && drawConfig_.testOnly;
    results.hasResignTest = resignConfig_.active && resignConfig_.testOnly;
    
    if (results.hasDrawTest) {
        results.drawResult = {
            {.key = "label", .value = "draw"},
            {.key = "total", .value = std::to_string(drawStats_.totalGames)},
            {.key = "correct", .value = std::to_string(drawStats_.correctDecisions)},
            {.key = "incorrect", .value = std::to_string(drawStats_.incorrectDecisions)},
            {.key = "saved", .value = QaplaHelpers::formatMs(drawStats_.savedTimeMs)},
            {.key = "total_time", .value = QaplaHelpers::formatMs(drawStats_.totalTimeMs)}
        };
    }
    
    if (results.hasResignTest) {
        results.resignResult = {
            {.key = "label", .value = "resign"},
            {.key = "total", .value = std::to_string(resignStats_.totalGames)},
            {.key = "correct", .value = std::to_string(resignStats_.correctDecisions)},
            {.key = "incorrect", .value = std::to_string(resignStats_.incorrectDecisions)},
            {.key = "saved", .value = QaplaHelpers::formatMs(resignStats_.savedTimeMs)},
            {.key = "total_time", .value = QaplaHelpers::formatMs(resignStats_.totalTimeMs)}
        };
    }
    
    return results;
}

void AdjudicationManager::printTestResult(std::ostream& out) const {
    auto results = computeTestResults();
    
    if (!results.hasDrawTest && !results.hasResignTest) {
        return;
    }
    
    auto printResult = [&](const std::vector<TestResultEntry>& result) {
        std::string label;
        for (const auto& entry : result) {
            if (entry.key == "label") {
                label = entry.value;
                break;
            }
        }
        
        out << "adjudicate " << std::setw(6) << std::left << label;
        for (const auto& entry : result) {
            if (entry.key == "total") {
                out << " total     " << std::setw(6) << entry.value;
            } else if (entry.key == "correct") {
                out << " correct   " << std::setw(6) << entry.value;
            } else if (entry.key == "incorrect") {
                out << " incorrect " << std::setw(6) << entry.value;
            } else if (entry.key == "saved") {
                out << " saved     " << std::setw(10) << entry.value;
            } else if (entry.key == "total_time") {
                out << " total     " << entry.value;
            }
        }
        out << "\n";
    };
    
    out << "Adjudication test results:\n";
    if (results.hasDrawTest) {
        printResult(results.drawResult);
    }
    if (results.hasResignTest) {
        printResult(results.resignResult);
    }
    out << "\n" << std::flush;
}

} // namespace QaplaTester
