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

#include <chrono>
#include <ctime>
#include "timer.h"
#include "pgn-io.h"
#include "time-control.h"
#include "game-state.h"

void PgnIO::initialize(const std::string& event) {
    event_ = event;
    if (!options_.append) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        std::ofstream out(options_.file, std::ios::trunc);
        // Leert die Datei – kein weiterer Inhalt nötig
    }
}

void PgnIO::saveTags(std::ostream& out, const GameRecord& game) {
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
		out << "[Round \"" + std::to_string(game.getRound()) + "\"]\n";
        const auto [cause, result] = game.getGameResult();
        out << "[Result \"" << to_string(result) << "\"]\n";
	    std::string termination = "nomrmal";
        switch (cause) {
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
                termination = "rules infraction";
                break;
            case GameEndCause::Timeout:
			    termination = "time forfeit";
			    break;

            default:
                termination = "normal";
                break;
        }
	    out << "[Termination \"" <<  termination << "\"]\n";

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

void PgnIO::saveMove(std::ostream& out, const std::string& san, 
    const MoveRecord& move, uint32_t plyIndex, bool isWhiteStart) {
    
    bool shouldPrintMoveNumber = (plyIndex % 2 == 0 && isWhiteStart) || (plyIndex % 2 == 1 && !isWhiteStart);
    if (shouldPrintMoveNumber) {
        out << ((plyIndex / 2) + 1) << ". ";
    }

    out << san;

    bool hasAnnotation = (options_.includeEval && (move.scoreCp || move.scoreMate))
        || (options_.includeDepth && move.depth > 0)
        || (options_.includeClock && move.timeMs > 0)
        || (options_.includePv && !move.pv.empty());

    if (hasAnnotation) {
        out << " {";

        bool needSpace = false;

        if (options_.includeEval && (move.scoreCp || move.scoreMate)) {
            out << move.evalString();
            needSpace = true;
        }

        if (options_.includeDepth && move.depth > 0) {
            out << "/" << move.depth;
            needSpace = true;
        }

        if (options_.includeClock && move.timeMs > 0) {
            if (needSpace) out << " ";
            out << std::fixed << std::setprecision(2) << (move.timeMs / 1000.0) << "s";
            needSpace = true;
        }

        if (options_.includePv && !move.pv.empty()) {
            if (needSpace) out << " ";
            out << move.pv;
        }

        out << "}";
    }

    out << " ";
}

void PgnIO::saveGame(const GameRecord& game) {
	if (options_.file.empty()) {
        return;
	}
    if (options_.saveAfterMove) {
        throw std::runtime_error("saveAfterMove not yet supported");
    }
    const auto [cause, result] = game.getGameResult();

    if (options_.onlyFinishedGames) {
        if (result == GameResult::Unterminated || cause == GameEndCause::Ongoing) {
            return;
        }
    }

    std::lock_guard<std::mutex> lock(fileMutex_);
    std::ofstream out(options_.file, std::ios::app);
    if (!out) {
        throw std::runtime_error("Failed to open PGN file: " + options_.file);
    }

    GameState state;
	state.setFen(game.getStartPos(), game.getStartFen());

    saveTags(out, game);

    const auto& history = game.history();
    bool isWhiteStart = (game.history().size() % 2 == 0) ? game.isWhiteToMove() : !game.isWhiteToMove();
    if (!isWhiteStart) {
        out << "1... ";
    }
    for (size_t i = 0; i < history.size(); ++i) {
        auto moveRecord = history[i];
        auto lan = moveRecord.lan;
		auto move = state.stringToMove(lan, true);
        auto san = state.moveToSan(move);
		state.doMove(move);
        saveMove(out, san, history[i], static_cast<uint32_t>(i), isWhiteStart);
    }

    out << to_string(std::get<1>(game.getGameResult())) << "\n\n";
}


