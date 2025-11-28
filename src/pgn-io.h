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
        bool skipEmptyGames = false;                       ///< Whether to skip games without moves and without custom FEN
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
     * @brief Processes the lines of the PGN file.
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
