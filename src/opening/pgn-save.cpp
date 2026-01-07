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
 * @author Volker BÃ¶hm
 * @copyright Copyright (c) 2025 Volker BÃ¶hm
 */

#include "pgn-save.h"

#include "../base-elements/time-control.h"
#include "../chess-game/game-result.h"

#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>


namespace QaplaTester {

void PgnSave::initialize(const std::string& event, bool isResumingTournament) {
    event_ = event;
    // Only truncate the file if:
    // - append mode is disabled (overwrite mode)
    // - AND we're starting a fresh tournament (not resuming)
    if (!options_.append && !isResumingTournament) {
        std::scoped_lock lock(fileMutex_);
        std::ofstream out(options_.file, std::ios::trunc | std::ios::binary);
    }
}

void PgnSave::saveTags(std::ostream& out, const GameRecord& game) {
    out << "[White \"" << game.getWhiteEngineName() << "\"]\n";
    out << "[Black \"" << game.getBlackEngineName() << "\"]\n";

    if (!game.getStartPos()) {
        out << "[FEN \"" << game.getStartFen() << "\"]\n";
        out << "[SetUp \"1\"]\n";
    }
    else {
        out << "[SetUp \"0\"]\n";
    }
    if (!event_.empty()) {
        out << "[Event \"" << event_ << "\"]\n";
    }

    if (!options_.minimalTags) {
        auto now = std::chrono::system_clock::now();
        std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};

#ifdef _WIN32
        bool success = localtime_s(&tm, &nowTimeT) == 0;
#else
        bool success = localtime_r(&nowTimeT, &tm) != nullptr;
#endif

        if (success) {
            std::string date = std::format("{:04}.{:02}.{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            std::string time = std::format("{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec);

            out << "[EventDate \"" << date << "\"]\n";
            out << "[Time \"" << time << "\"]\n";
        }
        out << "[Round \"" + std::to_string(game.getTotalGameNo()) + "\"]\n";
        const auto [cause, result] = game.getGameResult();
        out << "[Result \"" << to_string(result) << "\"]\n";
        std::string termination = "normal";
        switch (cause) {
            case GameEndCause::Checkmate:
            case GameEndCause::Stalemate:
            case GameEndCause::DrawByRepetition:
            case GameEndCause::DrawByFiftyMoveRule:
            case GameEndCause::DrawByInsufficientMaterial:
                termination = "normal";
                break;
            case GameEndCause::Ongoing:
                termination = "unterminated";
                break;
            case GameEndCause::TerminatedByTester:
            case GameEndCause::Resignation:
            case GameEndCause::DrawByAgreement:
            case GameEndCause::Adjudication:
                termination = "adjudication";
                break;
            case GameEndCause::Disconnected:
            case GameEndCause::IllegalMove:
            case GameEndCause::Forfeit:
                termination = "rules infraction";
                break;
            case GameEndCause::Timeout:
                termination = "time forfeit";
                break;
            default:
                termination = "normal";
                break;
        }
        out << "[Termination \"" << termination << "\"]\n";

        const auto& tcWhite = game.getWhiteTimeControl();
        const auto& tcBlack = game.getBlackTimeControl();
        if (tcWhite == tcBlack) {
            out << "[TimeControl \"" << to_string(tcWhite) << "\"]\n";
        }
        else {
            out << "[TimeControlWhite \"" << to_string(tcWhite) << "\"]\n";
            out << "[TimeControlBlack \"" << to_string(tcBlack) << "\"]\n";
        }
        out << "[PlyCount \"" << game.history().size() << "\"]\n";
    }

    out << "\n";
}

void PgnSave::saveMove(std::ostream& out, const std::string& san, 
    const MoveRecord& move, uint32_t plyIndex, bool isWhiteStart) const {
    
    bool shouldPrintMoveNumber = (plyIndex % 2 == 0 && isWhiteStart) || (plyIndex % 2 == 1 && !isWhiteStart);
    if (shouldPrintMoveNumber) {
        out << ((plyIndex / 2) + 1) << ". ";
    }

    out << san;

    bool hasComment = (options_.includeEval && (move.scoreCp || move.scoreMate))
        || (options_.includeDepth && move.depth > 0)
        || (options_.includeClock && move.timeMs > 0)
        || (options_.includePv && !move.pv.empty());

    if (hasComment) {
        out << " {";
        std::string sep;

        if (options_.includeEval && (move.scoreCp || move.scoreMate)) {
            out << move.evalString();
            sep = " ";
        }

        if (options_.includeDepth && move.depth > 0) {
            out << "/" << move.depth;
            sep = " ";
        }

        if (options_.includeClock && move.timeMs > 0) {
            out << sep << std::fixed << std::setprecision(2) 
                << (static_cast<double>(move.timeMs) / 1000.0) << "s";
            sep = " ";
        }

        if (options_.includePv && !move.pv.empty()) {
            out << sep << move.pv;
        }

        out << "}";
    }

    out << " ";
}

void PgnSave::saveGameToStream(std::ostream& out, const GameRecord& game) {
    const auto [cause, result] = game.getGameResult();

    if (options_.onlyFinishedGames) {
        if (result == GameResult::Unterminated || cause == GameEndCause::Ongoing) {
            return;
        }
    }

    saveTags(out, game);

    const auto& history = game.history();
    if (!history.empty()) {
        MoveRecord::toStringOptions opts {
            .includeClock = options_.includeClock,
            .includeEval = options_.includeEval,
            .includePv = options_.includePv,
            .includeDepth = options_.includeDepth
        };
        std::string movesStr = game.movesToStringUpToPly(history.size(), opts);
        out << movesStr;
    }

    out << " " << to_string(std::get<1>(game.getGameResult())) << "\n\n";
}

void PgnSave::saveGame(const GameRecord& game) {
    if (options_.file.empty()) {
        return;
    }
    if (options_.saveAfterMove) {
        throw std::runtime_error("saveAfterMove not yet supported");
    }

    std::scoped_lock lock(fileMutex_);
    saveGame(options_.file, game);
}

void PgnSave::saveGame(const std::string& fileName, const GameRecord& game) {
    std::ofstream out(fileName, std::ios::app | std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open PGN file: " + fileName);
    }

    saveGameToStream(out, game);
}

} // namespace QaplaTester
