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

 #include "adjucation-manager.h"
 #include "string-helper.h"

std::pair<GameEndCause, GameResult> AdjudicationManager::adjudicateDraw(const GameRecord& game) const {
    if (!drawConfig || drawConfig->testOnly) {
		return { GameEndCause::Ongoing, GameResult::Unterminated };
    }
    const auto& moves = game.history();
    const auto& cfg = *drawConfig;

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
    if (!resignConfig || resignConfig->testOnly) {
        return { GameEndCause::Ongoing, GameResult::Unterminated };
    }
    const auto& moves = game.history();
    const auto& cfg = *resignConfig;

    if (cfg.requiredConsecutiveMoves <= 0 
        || cfg.centipawnThreshold <= 0 
        || moves.size() < static_cast<size_t>(2 * cfg.requiredConsecutiveMoves)) {
        return { GameEndCause::Ongoing, GameResult::Unterminated };
    }

    uint32_t consecutive = 0;

	auto& lastMove = moves.back();
    bool curLoosing = lastMove.scoreCp && *lastMove.scoreCp <= -cfg.centipawnThreshold;
    // If white to move, the last move is a black move. When last last move looses the opponent wins
    GameResult prospectiveResult = game.isWhiteToMove() == curLoosing ? GameResult::WhiteWins : GameResult::BlackWins;

    for (int i = static_cast<int>(moves.size()) - 1; i >= 0; --i) {
        const auto& move = moves[static_cast<uint32_t>(i)];

        if (curLoosing) {
            if (!move.scoreCp || *move.scoreCp > -cfg.centipawnThreshold) break;
        } else if (cfg.twoSided) {
            if (!move.scoreCp || *move.scoreCp < cfg.centipawnThreshold) break;
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
	const auto& cfg = *drawConfig;
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

std::pair<GameResult, size_t> AdjudicationManager::findResignAdjudicationIndex(const GameRecord& game) const {
	const auto& cfg = *resignConfig;
    const auto& moves = game.history();
    if (cfg.requiredConsecutiveMoves <= 0 || cfg.centipawnThreshold <= 0 || moves.size() < 2 * cfg.requiredConsecutiveMoves) {
        return { GameResult::Unterminated , 0 };
    }

     // We must determine who moves first, since the game may start from a FEN with black to move
    bool wtm = wtmAtPly(game, 0);

    // Separate counters are required to independently track each side’s uninterrupted losing streak
    uint32_t wConsecutive = 0;
    uint32_t bConsecutive = 0;
    // Required to ensure that both sides satisfy their respective conditions when two-sided resign is active
    uint32_t requiredConsecutive = cfg.twoSided ? 2 * cfg.requiredConsecutiveMoves : cfg.requiredConsecutiveMoves;

    for (size_t i = 0; i < moves.size(); ++i) {
        const auto& move = moves[i];

        bool loosing = move.scoreCp && *move.scoreCp <= -cfg.centipawnThreshold;
        if (wtm) {
            wConsecutive = loosing ? wConsecutive + 1 : 0;
        } else {
            bConsecutive = loosing ? bConsecutive + 1 : 0;
        }
        
        if (cfg.twoSided) {
            bool winning = move.scoreCp && *move.scoreCp >= cfg.centipawnThreshold;
            if (wtm) {
                bConsecutive = winning ? bConsecutive + 1 : 0;
            } else {
                wConsecutive = winning ? wConsecutive + 1 : 0;
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

    if (drawConfig && drawConfig->testOnly) {
        drawStats.totalGames++;

        if (auto index = findDrawAdjudicationIndex(game)) {
            for (size_t i = *index + 1; i < moves.size(); ++i) drawStats.savedTimeMs += moves[i].timeMs;

            if (finalResult == GameResult::Draw) {
                drawStats.correctDecisions++;
            } else {
                drawStats.incorrectDecisions++;
                drawStats.failed.push_back(game);
            }
        }

        for (const auto& m : moves) drawStats.totalTimeMs += m.timeMs;
    }

    if (resignConfig && resignConfig->testOnly) {
        resignStats.totalGames++;
        auto [adjudicatedResult, index] = findResignAdjudicationIndex(game);
        if (adjudicatedResult != GameResult::Unterminated) {
            for (size_t i = index + 1; i < moves.size(); ++i) resignStats.savedTimeMs += moves[i].timeMs;

            if (finalResult == adjudicatedResult) {
                resignStats.correctDecisions++;
            } else {
                resignStats.incorrectDecisions++;
                resignStats.failed.push_back(game);
            }
        }

        for (const auto& m : moves) resignStats.totalTimeMs += m.timeMs;
    }
}

void AdjudicationManager::printTestResult(std::ostream& out) const {
    auto print = [&](const std::string& label, const AdjudicationTestStats& stats) {
        out << "adjudicate " << std::setw(6) << std::left << label
            << " total     " << std::setw(6) << stats.totalGames
            << " correct   " << std::setw(6) << stats.correctDecisions
            << " incorrect " << std::setw(6) << stats.incorrectDecisions
            << " saved     " << std::setw(10) << formatMs(stats.savedTimeMs)
            << " total     " << formatMs(stats.totalTimeMs)
            << "\n";
    };
    
    bool drawTest = drawConfig && drawConfig->testOnly;
    bool resignTest = resignConfig && resignConfig->testOnly;
    
    if (drawTest || resignTest) {
        out << "Adjudication test results:\n";
        if (drawTest) print("draw", drawStats);
        if (resignTest) print("resign", resignStats);
        out << std::endl;
    }
}
