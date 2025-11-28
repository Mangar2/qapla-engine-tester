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

#include "move-record.h"
#include "game-record.h"
#include "game-result.h"

#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <optional>
#include <functional>
#include <chrono>

namespace QaplaTester {

/**
 * @brief Represents a single trace entry during PGN file processing.
 */
struct PgnTraceEntry {
    enum class Level {
        Info,
        Warning,
        Error
    };

    Level level = Level::Info;
    std::string message;
    std::optional<size_t> gameNumber;   ///< Game number if applicable
    std::string context;                ///< Additional context (e.g., the raw game text)

    PgnTraceEntry(Level lvl, std::string msg, std::optional<size_t> game = std::nullopt, std::string ctx = "")
        : level(lvl), message(std::move(msg)), gameNumber(game), context(std::move(ctx)) {}

    [[nodiscard]] std::string levelToString() const {
        switch (level) {
            case Level::Info: return "INFO";
            case Level::Warning: return "WARNING";
            case Level::Error: return "ERROR";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] std::string toString() const {
        std::string result = "[" + levelToString() + "] " + message;
        if (gameNumber.has_value()) {
            result += " (game " + std::to_string(gameNumber.value()) + ")";
        }
        if (!context.empty()) {
            result += ": " + context;
        }
        return result;
    }
};

/**
 * @brief Result of parsing a single PGN tag.
 */
struct ParseTagResult {
    std::string key;                        ///< Tag key (empty if parsing failed)
    std::string value;                      ///< Tag value
    std::vector<std::string> traceLines;    ///< Trace lines if parsing errors occurred
    
    [[nodiscard]] bool isValid() const { return !key.empty(); }
};

/**
 * @brief Result of parsing a single PGN game.
 */
struct ParseGameResult {
    GameRecord game;                        ///< The parsed game record
    std::vector<std::string> traceLines;    ///< Trace lines from tag parsing errors
};

/**
 * @brief Result of processing file lines.
 */
struct ProcessFileLinesResult {
    std::vector<GameRecord> games;          ///< Successfully parsed games
    std::vector<std::string> traceLines;    ///< Trace lines from parsing errors
    bool completed = true;                  ///< Whether processing completed normally
};

/**
 * @brief Result of reading a PGN file with complete trace information.
 */
struct PgnReaderResult {
    std::vector<GameRecord> games;          ///< Successfully parsed games
    size_t errorCount = 0;                  ///< Number of games that failed to parse
    std::vector<PgnTraceEntry> trace;       ///< Complete trace of the reading process
    std::string filePath;                   ///< Path of the processed file
    bool fileOpened = false;                ///< Whether the file was successfully opened
    std::chrono::milliseconds duration{0};  ///< Time taken to process the file
    
    /**
     * @brief Calculates the error rate.
     * @return Error rate as a value between 0.0 and 1.0.
     */
    [[nodiscard]] double getErrorRate() const {
        auto total = games.size() + errorCount;
        return (total == 0) ? 0.0 : static_cast<double>(errorCount) / static_cast<double>(total);
    }
    
    /**
     * @brief Returns the total number of processed games.
     * @return Total count of games plus errors.
     */
    [[nodiscard]] size_t getTotalCount() const {
        return games.size() + errorCount;
    }

    /**
     * @brief Returns only error-level trace entries.
     * @return Vector of error trace entries.
     */
    [[nodiscard]] std::vector<PgnTraceEntry> getErrors() const {
        std::vector<PgnTraceEntry> errors;
        for (const auto& entry : trace) {
            if (entry.level == PgnTraceEntry::Level::Error) {
                errors.push_back(entry);
            }
        }
        return errors;
    }

    /**
     * @brief Returns the complete trace as a formatted string.
     * @return Formatted trace string.
     */
    [[nodiscard]] std::string getTraceString() const {
        std::string result;
        for (const auto& entry : trace) {
            result += entry.toString() + "\n";
        }
        return result;
    }
};

/**
 * @brief Thread-safe PGN input/output handler.
 */
class PgnIO {
public:
    /**
     * @brief PGN output configuration options.
     */
    struct Options {
        std::string file;
        bool append = true;
        bool onlyFinishedGames = true;
        bool minimalTags = false;
        bool saveAfterMove = false;
        bool includeClock = true;
        bool includeEval = true;
        bool includePv = true;
        bool includeDepth = true;
    };

    /**
     * @brief Parameters for loading games from a PGN file.
     */
    struct LoadParams {
        std::string filePath;                              ///< Path to the PGN file
        bool loadComments = false;                         ///< Whether to parse move comments
        std::optional<size_t> maxGames = std::nullopt;     ///< Maximum number of games to load (nullopt = all)
        size_t maxStoredErrorTraceEntries = 100;           ///< Maximum number of error trace entries to store
        std::function<bool(const GameRecord&, float)> gameCallback = nullptr;  ///< Optional progress callback
    };

    PgnIO() = default;

    /**
     * @brief Initializes the PGN output file depending on append mode.
     *        Clears file if append is false and not resuming an existing tournament.
     * @param event Event name for the tournament.
     * @param isResumingTournament If true, never truncates the file even in overwrite mode.
     *                            This should be true when loading existing tournament results.
     */
    void initialize(const std::string& event = "", bool isResumingTournament = false);

    /**
     * @brief Saves the given game record to the PGN file.
     * @param game Game record to be saved.
     */
    void saveGame(const GameRecord& game);

    /**
     * @brief Saves the given game record to the specified PGN file.
     * @param fileName Name of the PGN file to save to.
     * @param game Game record to be saved.
     */
    void saveGame(const std::string& fileName, const GameRecord& game);

    /**
     * @brief Saves the given game record to the provided output stream.
     * @param out Output stream to write to.
     * @param game Game record to be saved.
     */
    void saveGameToStream(std::ostream& out, const GameRecord& game);

    /**
     * @brief Loads games from a PGN file.
     * @param fileName Name of the PGN file to load from.
     * @param loadComments Whether to parse move comments or skip them for performance.
     * @param gameCallback Optional callback function called for each loaded game. 
     *                     Receives the GameRecord and progress percentage (0-100), returns true to continue loading, false to stop.
     *                     If nullptr, no callback is called.
     * @return Vector of parsed GameRecord instances.
     */
    std::vector<GameRecord> loadGames(const std::string& fileName, bool loadComments = false,
        const std::function<bool(const GameRecord&, float)>& gameCallback = nullptr);

    /**
     * @brief Loads games from a PGN file with detailed parameters and result.
     * @param params Load parameters including file path, limits, and options.
     * @return PgnReaderResult containing games, error count, and trace information.
     */
    PgnReaderResult loadGamesWithResult(const LoadParams& params);

    /**
     * @brief Gets the positions of games in the last loaded file.
     * @return Vector of stream positions for each game.
     */
    [[nodiscard]] const std::vector<std::streampos>& getGamePositions() const { return gamePositions_; }

    /**
     * @brief Loads a specific game from the previously loaded file by index.
     * @param index Index of the game to load.
     * @return Optional cleaned GameRecord if successful.
     */
    std::optional<GameRecord> loadGameAtIndex(size_t index);

    /**
     * @brief Gets the raw PGN text of a specific game by index.
     * @param index Index of the game to retrieve.
     * @return Optional string containing the raw PGN text if successful.
     */
    std::optional<std::string> getRawGameText(size_t index);

    /**
     * @brief Gets the filename of the currently loaded PGN file.
     * @return Reference to the current filename string.
     */
    [[nodiscard]] const std::string& getCurrentFileName() const { return currentFileName_; }

    /**
     * @brief Parses a single game from a PGN string.
     * @param pgnString The PGN formatted string containing a single game.
     * @return Parsed GameRecord instance.
     */
    static GameRecord parseGame(const std::string& pgnString);

	/**
	 * @brief Sets the options for PGN output.
	 * @param options New options to apply.
	 */
	void setOptions(const Options& options) {
		options_ = options;
	}

	static PgnIO& tournament() {
		static PgnIO instance;
		return instance;
	}

private:

    /**
     * @brief Writes PGN tag section for the given game.
     * @param out Output stream to write to.
     * @param game Game record to generate tags from.
     */
    void saveTags(std::ostream& out, const GameRecord& game);

    /**
     * @brief Writes a single PGN move with optional annotations.
     * @param out Output stream to write to.
	 * @param san Standard Algebraic Notation (SAN) of the move.
     * @param move Move to write.
     * @param plyIndex Zero-based ply index to determine move number and side.
     * @param isWhiteStart Whether white starts (relevant for proper numbering if not).
     */
    void saveMove(std::ostream& out, const std::string& san, const MoveRecord& move,
        uint32_t plyIndex, bool isWhiteStart) const;

    /**
     * @brief Parses a SAN move and attached annotations starting at a position.
     * @param tokens Token list from PGN input.
     * @param start Position to begin parsing from.
     * @param loadComments Whether to parse move comments or skip them.
     * @return Pair {MoveRecord, next position}. If no valid move, next == start.
     */
    static std::pair<MoveRecord, size_t> parseMove(const std::vector<std::string>& tokens, size_t start, bool loadComments = true);

    /**
     * @brief Parses a PGN tag line.
	 * @param tokens Tokenized line from PGN input.
     * @return ParseTagResult containing key, value, and any trace lines from errors.
     */
    static ParseTagResult parseTag(const std::vector<std::string>& tokens);

    /**
     * @brief Parses a PGN move line from tokens.
     * @param tokens Tokenized line from PGN input.
     * @param loadComments Whether to parse move comments or skip them.
     * @return Pair of move list and optional game result (1-0, 0-1, 1/2-1/2, *).
     */
    static std::pair<std::vector<MoveRecord>, std::optional<GameResult>> parseMoveLine(const std::vector<std::string>& tokens, bool loadComments = true);

    /**
     * @brief Skips a move-number indication like 12. or 23... starting at position.
     * @param tokens Token list from PGN input.
     * @param start Position to begin checking.
     * @return Next token position after move-number sequence.
     */
    static size_t skipMoveNumber(const std::vector<std::string>& tokens, size_t start);


    /**
    * @brief Skips a recursive variation in PGN notation starting at a given position.
    *        Recursive variations are enclosed in parentheses and can contain nested variations.
    * @param tokens Token list from PGN input.
    * @param start Position to begin checking.
    * @return Next token position after the recursive variation.
    *         If no valid variation is found, returns the start position.
    */
    static size_t skipRecursiveVariation(const std::vector<std::string>& tokens, size_t start);

    /**
     * @brief Parses a comment block following a SAN move and extracts metadata.
     * @param tokens Token list from PGN input.
     * @param start Position of the opening "{" token.
     * @param move MoveRecord to populate.
     * @return Position after closing "}" or unchanged on error.
     */
    static size_t parseMoveComment(const std::vector<std::string>& tokens, size_t start, MoveRecord& move);

    /**
     * @brief Parses a game end cause annotation from tokens and updates the GameRecord.
     * 
     * @param tokens Token list from PGN input.
     * @param start Position to begin checking.
     * @param cause Optional GameEndCause to populate if found.
     * @return Next token position after processing the annotation.
     */
    static size_t parseCauseAnnotation(const std::vector<std::string>& tokens, size_t start, 
        std::optional<GameEndCause>& cause);

    static void parseMateScore(const std::string& token, int32_t factor, MoveRecord& move);
    static void parseCpScore(const std::string& token, MoveRecord& move);
    
    /**
     * @brief Parses game-end information from comment tokens.
     *        Recognizes patterns like "White mates", "Black wins by resignation", "Draw by stalemate".
     * @param tokens Token list from PGN comment.
     * @param pos Current position in token list.
     * @param move MoveRecord to update with result and end cause.
     * @return Next position if pattern recognized, otherwise unchanged position.
     */
    static size_t parseGameEndInfo(const std::vector<std::string>& tokens, size_t pos, MoveRecord& move);
    static size_t parseGameEndInfo2(const std::vector<std::string>& tokens, size_t pos, MoveRecord& move);
    
    /**
     * @brief Collects tokens forming a game termination cause until delimiter.
     * @param tokens Token list from PGN comment.
     * @param pos Current position, updated to position after cause.
     * @return Concatenated cause string (e.g. "time forfeit", "50-move rule").
     */
    static std::string collectTerminationCause(const std::vector<std::string>& tokens, size_t& pos);

    /**
     * @brief Sets game result from parsed result token and move comments.
     * @param moves List of parsed moves.
     * @param parsedResult Optional result from PGN result token (1-0, 0-1, etc.).
     * @param game GameRecord to update with final result and end cause.
     */
    static void setGameResultFromParsedData(const std::vector<MoveRecord>& moves, 
                                           std::optional<GameResult> parsedResult, 
                                           GameRecord& game);

    /**
     * @brief Skips a comment block following a SAN move without parsing.
     * @param tokens Token list from PGN input.
     * @param start Position of the opening "{" token.
     * @return Position after closing "}" or unchanged on error.
     */
    static size_t skipMoveComment(const std::vector<std::string>& tokens, size_t start);

    /**
     * @brief Interprets known PGN tags and sets corresponding GameRecord fields.
     * @param game The GameRecord whose tags will be finalized.
     */
    static void finalizeParsedTags(GameRecord& game);

    /**
     * @brief Processes the lines of the PGN file in the while loop.
     * @param inFile The input file stream.
     * @param fileSize Size of the file for progress calculation.
     * @param params Load parameters containing options for parsing.
     * @return ProcessFileLinesResult containing games, trace lines, and completion status.
     */
    ProcessFileLinesResult processFileLines(std::ifstream& inFile, 
        std::streamsize fileSize, 
        const LoadParams& params);

    Options options_;
    std::vector<std::streampos> gamePositions_;  // Positions of games in the last loaded file
    std::string currentFileName_;  // Name of the last loaded file
    std::mutex mutex_;  // For thread safety
    std::mutex fileMutex_;
    std::string event_;
};

} // namespace QaplaTester
