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

#include "move-record.h"

#include "engine-event.h"

#include <string>
#include <optional>
#include <cassert>
#include <sstream>
#include <iomanip>

namespace QaplaTester {

void MoveRecord::clear() {
    original.clear();
    lan_.clear();
    san_.clear();
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
    info.clear();
    halfmoveNo_ = 0;
    engineId_.clear();
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
void MoveRecord::updateFromBestMove(uint32_t halfmoveNo, const std::string& engineId, 
    const EngineEvent& event, std::string lanMove, std::string sanMove,
    uint64_t computeStartTimestamp, uint32_t halfmoveClk) {
    halfmoveNo_ = halfmoveNo;
    engineId_ = engineId;
    if (event.bestMove) {
        original = *event.bestMove;
        lan_ = std::move(lanMove);
        san_ = std::move(sanMove);
    }
    halfmoveClock = halfmoveClk;
    timeMs = event.timestampMs - computeStartTimestamp;
}

/**
 * @brief Updates the move record with search information from an EngineEvent.
 * @param info SearchInfo containing various search metrics.
 */
void MoveRecord::updateFromSearchInfo(const SearchInfo& searchInfo, bool whitePovCorrection) {  // NOLINT(readability-function-cognitive-complexity)
    if (searchInfo.depth) {
        depth = *searchInfo.depth;
    }
    if (searchInfo.selDepth) {
        seldepth = *searchInfo.selDepth;
    }
    if (searchInfo.multipv) {
        multipv = *searchInfo.multipv;
    }
    if (searchInfo.nodes) {
        nodes = static_cast<uint64_t>(*searchInfo.nodes);
    }

    if (searchInfo.scoreCp) {
        scoreCp = whitePovCorrection ? -(*searchInfo.scoreCp) : *searchInfo.scoreCp;
        scoreMate.reset();
    }
    else if (searchInfo.scoreMate) {
        scoreMate = whitePovCorrection ? -(*searchInfo.scoreMate) : *searchInfo.scoreMate;
        scoreCp.reset();
    }

    if (!searchInfo.pv.empty()) {
        pv.clear();
        for (size_t i = 0; i < searchInfo.pv.size(); ++i) {
            if (i > 0) {
                pv += ' ';
            }
            pv += searchInfo.pv[i];
        }
    }
    infoUpdateCount++;
    // We keep the pv history, but everything else is overwritten
    if (!info.empty() && info.back().pv.empty()) {
        info.back().depth = depth;
        info.back().selDepth = seldepth;
        info.back().multipv = multipv;
        info.back().nodes = nodes;
        info.back().scoreCp = scoreCp;
        info.back().scoreMate = scoreMate;
        info.back().pv = searchInfo.pv;
        if (searchInfo.timeMs) {
            info.back().timeMs = searchInfo.timeMs;
        }
        if (searchInfo.hashFull) {
            info.back().hashFull = searchInfo.hashFull;
        }
        if (searchInfo.tbhits) {
            info.back().tbhits = searchInfo.tbhits;
        }
        if (searchInfo.cpuload) {
            info.back().cpuload = searchInfo.cpuload;
        }
        if (searchInfo.currMove) {
            info.back().currMove = searchInfo.currMove;
        }
        if (searchInfo.currMoveNumber) {
            info.back().currMoveNumber = searchInfo.currMoveNumber;
        }
        if (searchInfo.refutationIndex) {
            info.back().refutationIndex = searchInfo.refutationIndex;
        }
        if (!searchInfo.refutation.empty()) {
            info.back().refutation = searchInfo.refutation;
        }
    }
    else {
        info.push_back(searchInfo);
    }
}

void MoveRecord::updateFromHint(const std::string& ponderMove) {
    this->ponderMove = ponderMove;
}

/**
 * @brief Returns a string representation of the score.
 *
 * @return A string representing the score in centipawns or mate value.
 */
std::string MoveRecord::evalString() const {
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

MoveRecord MoveRecord::createMinimalCopy() const {
    MoveRecord result;
    result.lan_ = lan_;
    result.timeMs = timeMs;
    result.scoreCp = scoreCp;
    result.scoreMate = scoreMate;
    result.halfmoveClock = halfmoveClock;
    result.depth = depth;
    result.seldepth = seldepth;
    result.multipv = multipv;
    result.nodes = nodes;
    result.pv = pv;
    result.halfmoveNo_ = halfmoveNo_;
    result.engineId_ = engineId_;
    return result;
}

std::string MoveRecord::getGameEndText() const {
    std::ostringstream out;
    // Generate the game-end text
    if (endCause_ == GameEndCause::Checkmate) {
        // For checkmate, specify which side won
        if (result_ == GameResult::WhiteWins) {
            out << "White mates";
        } else if (result_ == GameResult::BlackWins) {
            out << "Black mates";
        }
    } else if (result_ == GameResult::Draw) {
        // For draws, use "Draw by" + termination string
        out << "Draw by " << gameEndCauseToPgnTermination(endCause_);
    } else if (result_ == GameResult::WhiteWins || result_ == GameResult::BlackWins) {
        // For other wins (resignation, timeout, etc.), specify winner
        std::string winner = (result_ == GameResult::WhiteWins) ? "White" : "Black";
        std::string cause = gameEndCauseToPgnTermination(endCause_);
        
        // Special handling for common cases
        if (endCause_ == GameEndCause::Resignation) {
            out << winner << " wins by resignation";
        } else if (endCause_ == GameEndCause::Timeout) {
            out << winner << " wins on time";
        } else if (endCause_ == GameEndCause::Forfeit) {
            out << winner << " wins by forfeit";
        } else {
            out << winner << " wins by " << cause;
        }
    }
    return out.str();
}

std::string MoveRecord::toString(const toStringOptions& opts) const { // NOLINT(readability-function-cognitive-complexity)
    std::ostringstream out;
    out << (san_.empty() ? lan_ : san_);

    bool hasComment = (opts.includeEval && (scoreCp || scoreMate))
        || (opts.includeDepth && depth > 0)
        || (opts.includeClock && timeMs > 0)
        || (opts.includePv && !pv.empty())
        || (result_ != GameResult::Unterminated);

    if (!hasComment && book) {
        out << " {book}";
    }

    if (hasComment) {
        out << " {";
        std::string sep;

        if (opts.includeEval && (scoreCp || scoreMate)) {
            out << evalString();
            sep = " ";
        }

        if (opts.includeDepth && depth > 0) {
            out << "/" << depth;
            sep = " ";
        }

        if (opts.includeClock && timeMs > 0) {
            out << sep << std::fixed << std::setprecision(2)
                << (static_cast<double>(timeMs) / 1000.0) << "s";
            sep = " ";
        }

        if (opts.includePv && !pv.empty()) {
            out << sep << pv;
            sep = " ";
        }

        // Add game-end information (cute-chess-cli style)
        if (result_ != GameResult::Unterminated) {
            if (!sep.empty()) {
                out << ",";
            }
            out << " " << getGameEndText();
        }

        out << "}";
    }

    return out.str();
}

} // namespace QaplaTester
