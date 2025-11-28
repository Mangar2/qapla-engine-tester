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

#include "pgn-io.h"

#include "timer.h"
#include "pgn-tokenizer.h"
#include "time-control.h"
#include "game-state.h"
#include "string-helper.h"

#include "game-result.h"

#include <chrono>
#include <ctime>

namespace QaplaTester {

void PgnIO::initialize(const std::string& event, bool isResumingTournament) {
    event_ = event;
    // Only truncate the file if:
    // - append mode is disabled (overwrite mode)
    // - AND we're starting a fresh tournament (not resuming)
    if (!options_.append && !isResumingTournament) {
        std::scoped_lock lock(fileMutex_);
        std::ofstream out(options_.file, std::ios::trunc | std::ios::binary);
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

static void updateGameEndFromTags(GameRecord& game, const std::map<std::string, std::string>& tags) {
auto [cause, result] = game.getGameResult();
    if (auto it = tags.find("Result"); it != tags.end()) {
        // We prefer game end information (1-0) over the Result tag, if both are conflicting.
        if (result  == GameResult::Unterminated) {
            if (it->second == "1-0") {
                result = GameResult::WhiteWins;
            }
            else if (it->second == "0-1") {
                result = GameResult::BlackWins;
            }
            else if (it->second == "1/2-1/2") {
                result = GameResult::Draw;
            }
            game.setGameEnd(GameEndCause::Unknown, result);
        }
    }
    if (cause == GameEndCause::Unknown && result != GameResult::Unterminated) {
        if (auto it = tags.find("Termination"); it != tags.end()) {
            const std::string& termStr = it->second;
            GameEndCause cause = GameEndCause::Unknown;

            if (termStr == "time forfeit") {
                cause = GameEndCause::Timeout;
            }
            else if (termStr == "rules infraction") {
                cause = GameEndCause::Forfeit;
            }
            else if (termStr == "adjudication") {
                cause = GameEndCause::Adjudication;
            }
            else if (termStr == "unterminated") {
                cause = GameEndCause::Ongoing;
            }
            else {
                cause = GameEndCause::Unknown;
            }

            game.setGameEnd(cause, result);        
        }
    }
}

static void finalizeParsedTags(GameRecord& game) {
    const auto& tags = game.getTags();

    if (auto it = tags.find("White"); it != tags.end()) {
        game.setWhiteEngineName(it->second);
    }
    if (auto it = tags.find("Black"); it != tags.end()) {
        game.setBlackEngineName(it->second);
    }
    if (auto it = tags.find("FEN"); it != tags.end()) {
        game.setFen(it->second);
    }
    if (auto it = tags.find("SetUp"); it != tags.end()) {
        if (it->second == "0") {
            // Nothing to do, this is the default
        }
    }
    if (auto it = tags.find("Round"); it != tags.end()) {
        if (auto round = QaplaHelpers::to_uint32(it->second)) {
            game.setGameInRound(*round);
        }
    }
   
    if (auto it = tags.find("TimeControl"); it != tags.end()) {
        TimeControl tc;
        tc.fromPgnTimeControlString(it->second); 
        game.setTimeControl(tc, tc);
    }
    if (auto itW = tags.find("TimeControlWhite"), itB = tags.find("TimeControlBlack");
        itW != tags.end() && itB != tags.end()) {
        TimeControl tcW;
        TimeControl tcB;
        tcW.fromPgnTimeControlString(itW->second);
        tcB.fromPgnTimeControlString(itB->second);
        game.setTimeControl(tcW, tcB);
    }
    updateGameEndFromTags(game, tags);

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

void PgnIO::saveGameToStream(std::ostream& out, const GameRecord& game) {
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

std::optional<std::string> PgnIO::getRawGameText(size_t index) {
    if (index >= gamePositions_.size() || currentFileName_.empty()) {
        return std::nullopt;
    }

    std::ifstream inFile(currentFileName_, std::ios::binary);
    if (!inFile) {
        return std::nullopt;
    }

    std::streampos startPos = gamePositions_[index];
    std::streampos endPos;

    if (index + 1 < gamePositions_.size()) {
        endPos = gamePositions_[index + 1];
    } else {
        // Last game, read to end of file
        inFile.seekg(0, std::ios::end);
        endPos = inFile.tellg();
    }

    std::streamsize length = endPos - startPos;
    if (length <= 0) {
        return std::nullopt;
    }

    inFile.seekg(startPos);
    std::string gameString(length, '\0');
    inFile.read(gameString.data(), length);

    if (!inFile) {
        return std::nullopt;
    }

    return gameString;
}

std::optional<GameRecord> PgnIO::loadGameAtIndex(size_t index) {
    auto gameString = getRawGameText(index);
    if (!gameString) {
        return std::nullopt;
    }

    // Parse the game
    GameRecord record = parseGame(*gameString);

    // Clean the record
    GameState gameState;
    GameRecord cleanRecord = gameState.setFromGameRecordAndCopy(record);

    // Check validity
    bool hasFen = !cleanRecord.getStartFen().empty() && cleanRecord.getStartFen() != "startpos";
    bool hasMoves = !cleanRecord.history().empty();

    if (!hasFen && !hasMoves) {
        return std::nullopt;
    }

    return cleanRecord;
}

void PgnIO::saveGame(const GameRecord& game) {
	if (options_.file.empty()) {
        return;
	}
    if (options_.saveAfterMove) {
        throw std::runtime_error("saveAfterMove not yet supported");
    }

    std::scoped_lock lock(fileMutex_);
    saveGame(options_.file, game);
}

void PgnIO::saveGame(const std::string& fileName, const GameRecord& game) {
    std::ofstream out(fileName, std::ios::app | std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open PGN file: " + fileName);
    }

    saveGameToStream(out, game);
}

namespace {

size_t skipMoveNumber(const std::vector<std::string>& tokens, size_t start) {
    if (start >= tokens.size()) {
        return start;
    }

    const std::string& first = tokens[start];
    if (first.empty()) {
        return start;
    }

    // Check if it starts with digits
    size_t i = 0;
    while (i < first.size() && std::isdigit(static_cast<unsigned char>(first[i])) != 0) {
        ++i;
    }
    if (i == 0) {
        return start; // Doesn't start with digit
    }

    // Now check for dots
    while (i < first.size() && first[i] == '.') {
        ++i;
    }
    if (i < first.size()) {
        return start; // Extra characters after dots
    }

    // Now skip additional dots in subsequent tokens
    size_t pos = start + 1;
    while (pos < tokens.size() && tokens[pos] == ".") {
        ++pos;
    }
    return pos;
}

size_t skipRecursiveVariation(const std::vector<std::string>& tokens, size_t start) {
	if (start >= tokens.size() || tokens[start] != "(") {
		return start;
	}
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

ParseTagResult parseTag(const std::vector<std::string>& tokens) {
    ParseTagResult result;
    
    auto tokensToString = [&tokens]() {
        std::string s;
        for (const auto& t : tokens) {
            if (!s.empty()) {
                s += " ";
            }
            s += t;
        }
        return s;
    };
    
    if (tokens.size() != 4) {
        result.traceLines.push_back("Invalid tag: expected 4 tokens, got " + 
            std::to_string(tokens.size()) + ": " + tokensToString());
        return result;
    }
    if (tokens[0] != "[" || tokens[3] != "]") {
        result.traceLines.push_back("Invalid tag: missing brackets: " + tokensToString());
        return result;
    }
    if (tokens[2].size() < 2 || tokens[2].front() != '"' || tokens[2].back() != '"') {
        result.traceLines.push_back("Invalid tag: value not quoted properly: " + tokensToString());
        return result;
    }

    result.key = tokens[1];
    result.value = tokens[2].substr(1, tokens[2].size() - 2);
    return result;
}

void parseMateScore(const std::string& token, int32_t factor, MoveRecord& move) {
    size_t i = 0;
    while (i < token.size() && (std::isdigit(static_cast<unsigned char>(token[i])) == 0)) {
        ++i;
    }
    
    if (auto mateValue = QaplaHelpers::to_int(std::string_view(token).substr(i))) {
        move.scoreMate = *mateValue * factor;
    }
}

void parseCpScore(const std::string& token, MoveRecord& move) {
    // Centipawn score, e.g. +0.21 or -1.5
    auto cp =QaplaHelpers::to_double(token); 
    if (cp) {
        move.scoreCp = static_cast<int>(*cp * 100.0);
    }
}

size_t parseCauseAnnotation(const std::vector<std::string>& tokens, size_t start, std::optional<GameEndCause>& cause) {
    if (tokens[start] != "{") {
        return start;
    }
    size_t pos = start + 1;
    std::string causeStr;

    for (int i = 0; i < 3 && pos < tokens.size(); i++, pos++) {
        causeStr += tokens[pos];
        pos++;
        cause = tryParseGameEndCause(causeStr);
        if (cause) { 
            break;
        }
    }
    if (cause && tokens[pos] == "}") {
        return pos + 1;
    }
    return start;
}

std::string collectTerminationCause(const std::vector<std::string>& tokens, size_t& pos) {
    std::string causeStr;
    while (pos < tokens.size() && tokens[pos] != "}" && tokens[pos] != ",") {
        if (!causeStr.empty()) {
            causeStr += " ";
        }
        causeStr += tokens[pos];
        ++pos;
    }
    return causeStr;
}


size_t parseGameEndInfo(const std::vector<std::string>& tokens, size_t pos, MoveRecord& move) {
    const std::string& tok = tokens[pos];
    if (pos + 1 >= tokens.size()) {
        return pos;
    }
    
    if (tok == "White") {
        const std::string& nextTok = tokens[pos + 1];
        if (nextTok == "mates") {
            move.result_ = GameResult::WhiteWins;
            move.endCause_ = GameEndCause::Checkmate;
            return pos + 2;
        }
        if (nextTok == "wins" && pos + 2 < tokens.size() && tokens[pos + 2] == "by") {
            move.result_ = GameResult::WhiteWins;
            size_t causePos = pos + 3;
            auto cause = tryParseGameEndCause(collectTerminationCause(tokens, causePos));
            if (cause) {
                move.endCause_ = *cause;
            }
            return causePos;
        }
    }
    else if (tok == "Black") {
        const std::string& nextTok = tokens[pos + 1];
        if (nextTok == "mates") {
            move.result_ = GameResult::BlackWins;
            move.endCause_ = GameEndCause::Checkmate;
            return pos + 2;
        }
        if (nextTok == "wins" && pos + 2 < tokens.size() && tokens[pos + 2] == "by") {
            move.result_ = GameResult::BlackWins;
            size_t causePos = pos + 3;
            auto cause = tryParseGameEndCause(collectTerminationCause(tokens, causePos));
            if (cause) {
                move.endCause_ = *cause;
            }
            return causePos;
        }
    }
    else if (tok == "Draw" && tokens[pos + 1] == "by") {
        move.result_ = GameResult::Draw;
        size_t causePos = pos + 2;
        auto cause = tryParseGameEndCause(collectTerminationCause(tokens, causePos));
        if (cause) {
            move.endCause_ = *cause;
        }
        return causePos;
    }
    
    return pos;
}

size_t parseMoveComment(const std::vector<std::string>& tokens, size_t start, MoveRecord& move) { // NOLINT(readability-function-cognitive-complexity)
    if (tokens[start] != "{") {
        return start;
    }

    std::string pv;
    size_t pos = start + 1;

    // PGN comment format: {eval/depth time pv, game-end-info}
    // Example: {+0.31/14 0.89s e2e4 d7d5, White mates}
    for (; pos < tokens.size() && tokens[pos] != "}"; ++pos) {
        const std::string& tok = tokens[pos];
        if (tok.empty()) {
            continue;
        }

        size_t nextPos = parseGameEndInfo(tokens, pos, move);
        if (nextPos != pos) {
            // nextPos is pointing to the next token. 
            // As ++pos will advance pos again, we need to decrement by 1 here.
            pos = nextPos - 1;
            continue;
        }

        if (tok[0] == 'M' || tok[0] == '#') {
            parseMateScore(tok, 1, move);
            continue;
        }
        if (tok.length() >= 2 && (tok[1] == 'M' || tok[1] == '#')) {
            parseMateScore(tok, tok[0] == '+' ? 1 : -1, move);
            continue;
        }
        if (tok[0] == '+' || tok[0] == '-') {
            parseCpScore(tok, move);
            continue;
        }
        if (tok == "/") {
            if (pos + 1 < tokens.size()) {
                if (auto depth = QaplaHelpers::to_int(tokens[pos + 1])) {
                    move.depth = static_cast<uint32_t>(*depth < 0 ? 0 : *depth);
                    ++pos;
                }
            }
            continue;
        }
        if (tok == ",") {
            // Ignore commas (we use one as separator before game-end info, 
            // but accept them anywhere for robustness)
            continue;
        }
        if (tok.ends_with("s")) {
            if (auto seconds = QaplaHelpers::to_double(tok.substr(0, tok.size() - 1))) {
                constexpr double msPerSecond = 1000.0;
                move.timeMs = static_cast<uint64_t>(*seconds * msPerSecond);
                continue;
            }
        }
        // All remaining tokens in a comment are PV moves until we either hit } or ","
        if (!pv.empty()) {
            pv += " ";
        }
        pv += tok;
    }

    move.pv = std::move(pv);
    if (pos < tokens.size() && tokens[pos] == "}") {
        ++pos;
    }
    return pos;
}

size_t skipMoveComment(const std::vector<std::string>& tokens, size_t start) {
    if (tokens[start] != "{") {
        return start;
    }

    size_t pos = start + 1;
    while (pos < tokens.size() && tokens[pos] != "}") {
        ++pos;
    }
    if (pos < tokens.size() && tokens[pos] == "}") {
        ++pos;
    }
    return pos;
}

std::pair<MoveRecord, size_t> parseMove(
    const std::vector<std::string>& tokens, size_t start, bool loadComments) {
    
    size_t pos = skipMoveNumber(tokens, start);
    if (pos >= tokens.size()) {
        return { {}, pos };
    }

    MoveRecord move;
    move.san_ = tokens[pos];
    ++pos;

    while (pos < tokens.size()) {
        const std::string& tok = tokens[pos];

        if (tok[0] == '$') {
            if (tok.size() > 1 && std::isdigit(static_cast<unsigned char>(tok[1])) != 0) {
                move.nag = tok;
            }
            ++pos;
        }
        else if (tok == "{") {
            if (loadComments) {
                pos = parseMoveComment(tokens, pos, move);
            } else {
                pos = skipMoveComment(tokens, pos);
            }
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

void setGameResultFromParsedData(const std::vector<MoveRecord>& moves, 
                                        std::optional<GameResult> parsedResult, 
                                        GameRecord& game) {
    auto [cause, result] = game.getGameResult();
    // This block is unneccessary in current calling order (it is called before the tags are concidered)
    // but kept if calling order changes in future
    if (parsedResult && *parsedResult != result) {
        result = *parsedResult;
        cause = GameEndCause::Ongoing;
    }
    // Game-end info in move comment is more specific than Result tag
    if (!moves.empty() && moves.back().result_ != GameResult::Unterminated) {
        if (result == GameResult::Unterminated || result == moves.back().result_) {
            result = moves.back().result_;
            cause = moves.back().endCause_;
        }
    }
    if (result != GameResult::Unterminated && cause == GameEndCause::Ongoing) {
        cause = GameEndCause::Unknown;
    }
    game.setGameEnd(cause, result);
}

std::pair<std::vector<MoveRecord>, std::optional<GameResult>> 
parseMoveLine(const std::vector<std::string>& tokens, bool loadComments) { // NOLINT(readability-function-cognitive-complexity)
    std::vector<MoveRecord> moves;
    std::optional<GameResult> result;
    std::optional<GameEndCause> cause;
    size_t pos = 0;

    while (pos < tokens.size()) {
        const auto& tok = tokens[pos];
        if (tok == "1-0") {
            return { moves, GameResult::WhiteWins };
        }
        if (tok == "0-1") {
            return { moves, GameResult::BlackWins };
        }
        if (tok == "1/2-1/2") {
            return { moves, GameResult::Draw };
        }
        if (tok == "*") {
            return { moves, GameResult::Unterminated };
        }
        // Check for spaced-out results
        if (pos + 2 < tokens.size()) {
            if (tok == "1" && tokens[pos+1] == "-" && tokens[pos+2] == "0") {
                return { moves, GameResult::WhiteWins };
            }
            if (tok == "0" && tokens[pos+1] == "-" && tokens[pos+2] == "1") {
                return { moves, GameResult::BlackWins };
            }
            if (tok == "1" && tokens[pos+1] == "/" && (tokens[pos+2] == "2-1" || tokens[pos+2] == "2")) {
                return { moves, GameResult::Draw };
            }
        }

        auto causePos = parseCauseAnnotation(tokens, pos, cause);
        if (causePos != pos) {
            pos = causePos;
            continue;
        }

        auto [move, nextPos] = parseMove(tokens, pos, loadComments);
        if (!move.san_.empty()) {
            moves.push_back(move);
        }
        pos = nextPos;

    }

    return { moves, result };
}

} // namespace

std::vector<GameRecord> PgnIO::loadGames(const std::string& fileName, bool loadComments,
    const std::function<bool(const GameRecord&, float)>& gameCallback) 
{
    LoadParams params;
    params.filePath = fileName;
    params.loadComments = loadComments;
    params.maxGames = std::nullopt;
    params.maxStoredErrorTraceEntries = 0;
    params.gameCallback = gameCallback;
    
    auto result = loadGamesWithResult(params);
    return std::move(result.games);
}

PgnReaderResult PgnIO::loadGamesWithResult(const LoadParams& params)
{
    PgnReaderResult result;
    result.filePath = params.filePath;
    
    auto startTime = std::chrono::steady_clock::now();
    
    std::ifstream inFile(params.filePath, std::ios::binary);
    if (!inFile) {
        result.fileOpened = false;
        return result;
    }
    result.fileOpened = true;

    // Get file size for progress calculation
    inFile.seekg(0, std::ios::end);
    std::streamsize fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    currentFileName_ = params.filePath;
    gamePositions_.clear();

    gamePositions_.push_back(inFile.tellg());

    auto processResult = processFileLines(inFile, fileSize, params);
    
    result.games = std::move(processResult.games);
    
    // Convert trace lines to PgnTraceEntry entries
    for (const auto& traceLine : processResult.traceLines) {
        result.trace.emplace_back(PgnTraceEntry::Level::Warning, traceLine);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return result;
}

GameRecord PgnIO::parseGame(const std::string& pgnString) { // NOLINT(readability-function-cognitive-complexity)
    GameRecord game;
    auto tokens = PgnTokenizer::tokenize(pgnString);
    long long pos = 0;

    while (static_cast<size_t>(pos) < tokens.size()) {
        if (tokens[pos] == "[") {
            // Parse tag (assumes 4 tokens: [, key, value, ])
            if (static_cast<size_t>(pos) + 3 < tokens.size()) {
                std::vector<std::string> tagTokens(tokens.begin() + pos, tokens.begin() + pos + 4);
                auto tagResult = parseTag(tagTokens);
                if (tagResult.isValid()) {
                    game.setTag(tagResult.key, tagResult.value);
                }
                pos += 4;
            } else {
                pos++; // Skip invalid
            }
        } else {
            std::vector<std::string> moveTokens(tokens.begin() + pos, tokens.end());
            auto [moves, result] = parseMoveLine(moveTokens, true);
            for (const auto& move : moves) {
                game.addMove(move);
            }
            setGameResultFromParsedData(moves, result, game);
            // We prefer game end information (1-0) over the Result tag, if both are conflicting.
            if (!moves.empty() && moves.back().result_ != GameResult::Unterminated) {
                auto [cause, curResult] = game.getGameResult();
                if (curResult == GameResult::Unterminated || curResult == moves.back().result_) {
                    game.setGameEnd(moves.back().endCause_, moves.back().result_);
                }
            }
            pos = static_cast<long long>(tokens.size());
            if (game.nextMoveIndex() > 2000) {
                break;
            }
        }
    }

    finalizeParsedTags(game);
    // Marks the game as unchanged after loading. This enables us to detect, if a game was modified later.
    game.clearChangeTracker();
    return game;
}

namespace {

/**
 * @brief Internal class for parsing PGN files line by line.
 * 
 * This class encapsulates all state needed during file parsing,
 * making the code cleaner and easier to maintain.
 */
class PgnFileParser {
public:
    PgnFileParser(std::ifstream& inFile, 
                  std::streamsize fileSize,
                  const PgnIO::LoadParams& params,
                  std::vector<std::streampos>& gamePositions)
        : inFile_(inFile)
        , fileSize_(fileSize)
        , params_(params)
        , gamePositions_(gamePositions) {}

    ProcessFileLinesResult parse() {
        std::string line;
        
        while ((currentPos_ = inFile_.tellg()), std::getline(inFile_, line)) {
            auto tokens = PgnTokenizer::tokenize(line);
            if (tokens.empty()) {
                continue;
            }

            if (tokens[0] == "[") {
                if (!processTagSection(tokens)) {
                    return result_;
                }
            } else {
                processMoveSection(tokens);
            }
        }
        
        finalizeLastGame();
        return result_;
    }

private:
    bool processTagSection(const std::vector<std::string>& tokens) {
        if (inMoveSection_) {
            if (!finalizeCurrentGame()) {
                return false;
            }
        }
        
        auto tagResult = parseTag(tokens);
        applyTag(tagResult);
        collectTraceLines(tagResult);
        return true;
    }

    void processMoveSection(const std::vector<std::string>& tokens) {
        auto [moves, parseResult] = parseMoveLine(tokens, params_.loadComments);
        for (const auto& move : moves) {
            currentGame_.addMove(move);
        }
        setGameResultFromParsedData(moves, parseResult, currentGame_);
        inMoveSection_ = true;
    }

    void applyTag(const ParseTagResult& tagResult) {
        if (tagResult.isValid()) {
            currentGame_.setTag(tagResult.key, tagResult.value);
            if (QaplaHelpers::to_lowercase(tagResult.key) == "fen") {
                currentGame_.setFen(tagResult.value);
            }
        }
    }

    void collectTraceLines(const ParseTagResult& tagResult) {
        if (tagResult.traceLines.empty() || 
            result_.traceLines.size() >= params_.maxStoredErrorTraceEntries) {
            return;
        }
        for (const auto& traceLine : tagResult.traceLines) {
            if (result_.traceLines.size() >= params_.maxStoredErrorTraceEntries) {
                break;
            }
            result_.traceLines.push_back("Game " + std::to_string(gameNumber_ + 1) + ": " + traceLine);
        }
    }

    bool finalizeCurrentGame() {
        finalizeParsedTags(currentGame_);
        
        // Skip empty games if requested
        if (params_.skipEmptyGames && isEmptyGame(currentGame_)) {
            resetForNextGame();
            return true;
        }
        
        result_.games.push_back(std::move(currentGame_));
        
        float progress = calculateProgress();
        if (params_.gameCallback && !params_.gameCallback(result_.games.back(), progress)) {
            result_.completed = false;
            return false;
        }
        
        if (params_.maxGames && result_.games.size() >= *params_.maxGames) {
            result_.completed = false;
            return false;
        }
        
        resetForNextGame();
        return true;
    }
    
    void resetForNextGame() {
        gameNumber_++;
        inMoveSection_ = false;
        currentGame_ = {};
        gamePositions_.push_back(currentPos_);
    }
    
    [[nodiscard]] static bool isEmptyGame(const GameRecord& game) {
        return game.history().empty() && game.getStartPos();
    }

    void finalizeLastGame() {
        if (!inMoveSection_ && currentGame_.getTags().empty()) {
            return;
        }
        if (params_.maxGames && result_.games.size() >= *params_.maxGames) {
            return;
        }
        
        finalizeParsedTags(currentGame_);
        
        // Skip empty games if requested
        if (params_.skipEmptyGames && isEmptyGame(currentGame_)) {
            return;
        }
        
        result_.games.push_back(std::move(currentGame_));
        
        if (params_.gameCallback) {
            params_.gameCallback(result_.games.back(), 100.0F);
        }
    }

    [[nodiscard]] float calculateProgress() const {
        if (fileSize_ <= 0) {
            return 0.0F;
        }
        return static_cast<float>(currentPos_) / static_cast<float>(fileSize_);
    }

    // Input references
    std::ifstream& inFile_;
    std::streamsize fileSize_;
    const PgnIO::LoadParams& params_;
    std::vector<std::streampos>& gamePositions_;

    // State
    ProcessFileLinesResult result_;
    GameRecord currentGame_;
    bool inMoveSection_ = false;
    size_t gameNumber_ = 0;
    std::streampos currentPos_;
};

} // anonymous namespace

ProcessFileLinesResult PgnIO::processFileLines(std::ifstream& inFile, 
    std::streamsize fileSize, 
    const LoadParams& params) 
{
    PgnFileParser parser(inFile, fileSize, params, gamePositions_);
    return parser.parse();
}

} // namespace QaplaTester
