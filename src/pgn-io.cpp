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
#include "string-helper.h"

void PgnIO::initialize(const std::string& event) {
    event_ = event;
    if (!options_.append) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        std::ofstream out(options_.file, std::ios::trunc);
        // Leert die Datei � kein weiterer Inhalt n�tig
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
	    std::string termination = "normal";
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

void PgnIO::finalizeParsedTags(GameRecord& game) {
    const auto& tags = game.getTags();

    if (auto it = tags.find("White"); it != tags.end()) {
        game.setWhiteEngineName(it->second);
    }
    if (auto it = tags.find("Black"); it != tags.end()) {
        game.setBlackEngineName(it->second);
    }
    if (auto it = tags.find("FEN"); it != tags.end()) {
        game.setStartPosition(false, it->second, game.isWhiteToMove(), game.getWhiteEngineName(), game.getBlackEngineName());
    }
    if (auto it = tags.find("SetUp"); it != tags.end()) {
        if (it->second == "0") {
            game.setStartPosition(true, "startpos", game.isWhiteToMove(), game.getWhiteEngineName(), game.getBlackEngineName());
        }
    }
    if (auto it = tags.find("Round"); it != tags.end()) {
        try {
            game.setRound(static_cast<uint32_t>(std::stoi(it->second)));
        }
        catch (...) {}
    }
    if (auto it = tags.find("Result"); it != tags.end()) {
        GameResult result = GameResult::Unterminated;
        if (it->second == "1-0") result = GameResult::WhiteWins;
        else if (it->second == "0-1") result = GameResult::BlackWins;
        else if (it->second == "1/2-1/2") result = GameResult::Draw;
        game.setGameEnd(GameEndCause::Ongoing, result);
    }
    if (auto it = tags.find("TimeControl"); it != tags.end()) {
        TimeControl tc;
        tc.fromPgnTimeControlString(it->second); // erwartet eine parseTimeControl Funktion
        game.setTimeControl(tc, tc);
    }
    if (auto itW = tags.find("TimeControlWhite"), itB = tags.find("TimeControlBlack");
        itW != tags.end() && itB != tags.end()) {
		TimeControl tcW, tcB;
        tcW.fromPgnTimeControlString(itW->second);
        tcB.fromPgnTimeControlString(itB->second);
        game.setTimeControl(tcW, tcB);
    }
}

void PgnIO::saveMove(std::ostream& out, const std::string& san, 
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
		std::string sep = "";

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
        const auto& moveRecord = history[i];
        const auto& lan = moveRecord.lan;
		const auto& move = state.stringToMove(lan, true);
        const auto& san = state.moveToSan(move);
		state.doMove(move);
        saveMove(out, san, history[i], static_cast<uint32_t>(i), isWhiteStart);
    }

    out << to_string(std::get<1>(game.getGameResult())) << "\n\n";
}

std::vector<std::string> PgnIO::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    size_t i = 0;
    while (i < line.size()) {
        char c = line[i];

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }

        // Single-character self-delimiting tokens
        if (std::string{ "{}[]().*<>" }.find(c) != std::string::npos) {
            tokens.emplace_back(1, c);
            ++i;
            continue; 
        }

        // String token
        if (c == '"') {
            token.clear();
            token += '"';
            ++i;
            while (i < line.size()) {
                char ch = line[i++];
                token += ch;
                if (ch == '\\' && i < line.size()) {
                    token += line[i++]; // escaped char
                }
                else if (ch == '"') {
                    break;
                }
            }
            tokens.push_back(token);
            continue;
        }

        // NAG ($123)
        if (c == '$') {
            token = "$";
            ++i;
            while (i < line.size() && std::isdigit(line[i])) {
                token += line[i++];
            }
            tokens.push_back(token);
            continue;
        }

        // Integer or symbol (starts with digit or letter)
        if (std::isalnum(c)) {
            token.clear();
            token += c;
            ++i;
            while (i < line.size()) {
                char ch = line[i];
                if (std::isalnum(ch) || ch == '_' || ch == '+' || ch == '#' ||
                    ch == '=' || ch == ':' || ch == '-') {
                    token += ch;
                    ++i;
                }
                else {
                    break;
                }
            }
            tokens.push_back(token);
            continue;
        }

        // Period (special self-delimiting token)
        if (c == '.') {
            tokens.emplace_back(1, '.');
            ++i;
            continue;
        }

        // Unknown or unsupported character -> skip
        ++i;
    }
    return tokens;
}

size_t PgnIO::skipMoveNumber(const std::vector<std::string>& tokens, size_t start) {
    if (start >= tokens.size()) return start;

    const std::string& first = tokens[start];
    if (!std::all_of(first.begin(), first.end(), ::isdigit)) return start;

    size_t pos = start + 1;
    while (pos < tokens.size() && tokens[pos] == "." && pos - start <= 3) {
        ++pos;
    }
    return pos;
}

size_t PgnIO::skipRecursiveVariation(const std::vector<std::string>& tokens, size_t start) {
	if (start >= tokens.size() || tokens[start] != "(") return start;
	size_t pos = start + 1;
	int depth = 1;
	while (pos < tokens.size()) {
		if (tokens[pos] == "(") {
			++depth;
		}
		else if (tokens[pos] == ")") {
			--depth;
			if (depth == 0) {
				return pos + 1; 
			}
		}
		++pos;
	}
	return pos; 
}

std::pair<MoveRecord, size_t> PgnIO::parseMove(const std::vector<std::string>& tokens, size_t start) {
    
    size_t pos = skipMoveNumber(tokens, start);
    if (pos >= tokens.size()) return { {}, pos };

    MoveRecord move;
    move.san = tokens[pos];
    ++pos;

    while (pos < tokens.size()) {
        const std::string& tok = tokens[pos];

        if (tok[0] == '$') {
            if (tok.size() > 1 && std::isdigit(tok[1])) move.nag = tok;
            ++pos;
        }
        else if (tok == "{") {
            pos = parseMoveComment(tokens, pos, move);
        }
        else if (tok == "(") {
            pos = skipRecursiveVariation(tokens, pos);
        }
        else {
            break;
        }
    }

    return { move, pos };
}

std::pair<std::string, std::string> PgnIO::parseTag(const std::vector<std::string>& tokens) {
    if (tokens.size() != 4) return { "", "" };
    if (tokens[0] != "[" || tokens[3] != "]") return { "", "" };
    if (tokens[2].size() < 2 || tokens[2].front() != '"' || tokens[2].back() != '"') return { "", "" };

    std::string tagName = tokens[1];
    std::string tagValue = tokens[2].substr(1, tokens[2].size() - 2);
    return { tagName, tagValue };
}

size_t PgnIO::parseMoveComment(const std::vector<std::string>& tokens, size_t start, MoveRecord& move) {
    if (tokens[start] != "{") return start;

    std::string pv;
    std::string sep;
    size_t pos = start + 1;

    while (pos < tokens.size() && tokens[pos] != "}") {
        const std::string& tok = tokens[pos];

        if (tok.size() > 1 && (tok[0] == '+' || tok[0] == '-')) {
            if (tok[1] == '#') {
                // Mate score, e.g. +#3 or -#4
                try {
                    move.scoreMate = std::stoi(tok.substr(2)) * (tok[0] == '-' ? -1 : 1);
                }
                catch (...) {}
            }
            else {
                // Centipawn score, e.g. +0.21
                try {
                    double cp = static_cast<double>(std::stof(tok));
                    move.scoreCp = static_cast<int>(cp * 100.0);
                }
                catch (...) {}
            }
        }
        else if (tok.size() > 1 && tok[0] == '/') {
            try {
                int depth = std::stoi(tok.substr(1));
                move.depth = static_cast<uint32_t>(depth < 0 ? 0: depth);
            }
            catch (...) {}
        }
        else if (tok.ends_with("s")) {
            try {
                double seconds = std::stod(tok.substr(0, tok.size() - 1));
                move.timeMs = static_cast<uint64_t>(seconds * 1000);
            }
            catch (...) {}
        }
        else {
            if (!pv.empty()) pv += " ";
            pv += tok;
        }
        ++pos;
    }

    move.pv = std::move(pv);
    if (pos < tokens.size() && tokens[pos] == "}") ++pos;
    return pos;
}


std::pair<std::vector<MoveRecord>, std::optional<GameResult>> PgnIO::parseMoveLine(const std::vector<std::string>& tokens) {
    std::vector<MoveRecord> moves;
    std::optional<GameResult> result;
    size_t pos = 0;

    while (pos < tokens.size()) {
        const auto& tok = tokens[pos];
        if (tok == "1-0") return { moves, GameResult::WhiteWins };
        if (tok == "0-1") return { moves, GameResult::BlackWins };
        if (tok == "1/2-1/2") return { moves, GameResult::Draw };
        if (tok == "*") return { moves, GameResult::Unterminated };

        auto [move, nextPos] = parseMove(tokens, pos);
        if (!move.san.empty()) {
            moves.push_back(move);
        }
        pos = nextPos;

    }

    return { moves, result };
}

std::vector<GameRecord> PgnIO::loadGames(const std::string& fileName) {
    std::vector<GameRecord> games;
    std::ifstream inFile(fileName);
    if (!inFile) return games;

    GameRecord currentGame;
    std::string line;
    bool inMoveSection = false;

    while (std::getline(inFile, line)) {
        auto tokens = tokenize(line);
        if (tokens.size() == 0) continue;

        if (tokens[0] == "[") {
            // Wenn vorher Z�ge verarbeitet wurden, beginnt jetzt ein neues Spiel
            if (inMoveSection) {
                finalizeParsedTags(currentGame);
                games.push_back(std::move(currentGame));
                currentGame = GameRecord();
                inMoveSection = false;
            }
            auto [key, value] = parseTag(tokens);
            if (!key.empty()) currentGame.setTag(key, value);
            continue;
        }

        auto [moves, result] = parseMoveLine(tokens);
        for (const auto& move : moves) {
            currentGame.addMove(move);
        }
        if (result) {
            auto [cause, curResult] = currentGame.getGameResult();
            currentGame.setGameEnd(curResult == *result ? cause: GameEndCause::Ongoing, *result);
        }
        inMoveSection = true;
    }

    if (inMoveSection || !currentGame.getTags().empty()) {
        finalizeParsedTags(currentGame);
        games.push_back(std::move(currentGame));
    }

    return games;
}

