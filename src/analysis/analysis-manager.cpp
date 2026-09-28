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

#include "analysis-manager.h"

#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"
#include "../base-elements/string-helper.h"
#include "../game-manager/game-state.h"
#include "../opening/pgn-io.h"
#include "../opening/pgn-save.h"

#include <algorithm>
#include <format>

namespace QaplaTester {

namespace {

/**
 * @brief Fills the move and the original notation of every move of a game read from a PGN.
 * @return False if a move does not play out, in which case the game must not be used.
 */
bool fillPlayedMoveFields(GameRecord& game) {
    GameState state;
    if (!state.setFen(game.getStartPos(), game.getStartFen())) {
        return false;
    }
    for (auto& move : game.history()) {
        const auto parsed = state.stringToMove(move.lan_.empty() ? move.san_ : move.lan_, false);
        if (parsed.isEmpty()) {
            return false;
        }
        move.move = parsed;
        move.original = parsed.getLAN();
        state.doMove(parsed);
    }
    return true;
}

/**
 * @brief Prepares a game as it was read or handed in for the analysis.
 * @return The game, or nullopt if one of its moves does not play out.
 */
std::optional<GameRecord> prepareGame(const GameRecord& read) {
    // A game read from a PGN carries its moves in SAN, and the engine is sent the moves in
    // LAN: replaying the game once fills both, the way a tournament does it with its
    // openings. A game whose moves do not all play out comes back short and is left out
    // rather than analysed up to its broken move.
    GameState state;
    GameRecord game = state.setFromGameRecordAndCopy(read, std::nullopt, false);
    // The replay fills the notations but leaves the two fields a game played here would
    // carry: the parsed move, and the move as the mover sent it - which is what replaying a
    // game forward hands back to the players. A game read from a PGN has neither.
    if (!fillPlayedMoveFields(game)) {
        return std::nullopt;
    }
    if (game.history().empty() || game.history().size() != read.history().size()) {
        return std::nullopt;
    }
    return game;
}

void logLeftOut(size_t number, const std::string& source) {
    Logger::reportLogger().log(
        std::format("game {} of '{}' has a move that cannot be played, it is left out",
            number, source),
        TraceLevel::warning);
}

} // namespace

std::unique_ptr<PgnGameReader> AnalysisManager::openPgnFile() const {
    PgnIO::LoadParams params;
    params.filePath = pgnFile_;
    params.loadComments = true;
    params.skipEmptyGames = true;
    return std::make_unique<PgnGameReader>(params);
}

std::optional<GameRecord> AnalysisManager::readPlayableGame(PgnGameReader& reader,
    size_t& readCount, bool warn) const {
    while (maxGames_ == 0 || readCount < maxGames_) {
        auto read = reader.next();
        if (!read) {
            return std::nullopt;
        }
        ++readCount;
        if (auto game = prepareGame(*read)) {
            return game;
        }
        if (warn) {
            logLeftOut(readCount, pgnFile_);
        }
    }
    return std::nullopt;
}

size_t AnalysisManager::initialize(const AnalysisConfig& config) {
    std::scoped_lock lock(mutex_);
    direction_ = config.direction;
    games_.clear();
    reader_.reset();
    pgnFile_ = config.pgnFile;
    maxGames_ = config.maxGames;

    auto reader = openPgnFile();
    if (!reader->isOpen()) {
        throw AppError::makeInvalidParameters(std::format(
            "Give a readable PGN file as analysis.pgn; '{}' could not be opened.", config.pgnFile));
    }

    gameCount_ = 0;
    size_t readCount = 0;
    while (readPlayableGame(*reader, readCount, true)) {
        ++gameCount_;
    }
    return gameCount_;
}

size_t AnalysisManager::initialize(std::vector<GameRecord> games, AnalysisDirection direction) {
    return adoptGames(std::move(games), direction, "the games handed in");
}

size_t AnalysisManager::adoptGames(std::vector<GameRecord> games, AnalysisDirection direction,
    const std::string& source) {
    direction_ = direction;

    std::scoped_lock lock(mutex_);
    pgnFile_.clear();
    maxGames_ = 0;
    reader_.reset();
    games_.clear();
    games_.reserve(games.size());
    for (size_t i = 0; i < games.size(); ++i) {
        auto game = prepareGame(games[i]);
        if (!game) {
            logLeftOut(i + 1, source);
            continue;
        }
        games_.push_back(std::move(*game));
    }
    gameCount_ = games_.size();
    return gameCount_;
}

size_t AnalysisManager::getGameCount() const {
    std::scoped_lock lock(mutex_);
    return gameCount_;
}

PgnSave& AnalysisManager::pgnSink() const {
    return pgnSink_ != nullptr ? *pgnSink_ : PgnSave::tournament();
}

void AnalysisManager::requirePerMoveLimit(const EngineConfig& engine) {
    const auto& timeControl = engine.getTimeControl();
    if (timeControl.moveTimeMs() || timeControl.depth() || timeControl.nodes()) {
        return;
    }

    const auto configured = timeControl.toPgnTimeControlString();
    throw AppError::makeInvalidParameters(std::format(
        "Set a per-move search limit on engine '{}': tc=movetime(ms):500, tc=depth:12 or "
        "tc=nodes:200000. An analysis gives every position the same limit, so the time control "
        "it currently has ({}) has nothing to apply to.",
        engine.getName(), configured.empty() ? "none" : configured));
}

void AnalysisManager::startRun(const EngineConfig& engine) {
    requirePerMoveLimit(engine);

    std::scoped_lock lock(mutex_);
    timeControl_ = engine.getTimeControl();
    engineName_ = engine.getName();
    nextIndex_ = 0;
    finishedCount_ = 0;
    playersInFlight_.clear();
    // Each run reads the file from its start again: every engine analyses every game.
    if (!pgnFile_.empty()) {
        reader_ = openPgnFile();
        gamesRead_ = 0;
    }
}

std::optional<GameRecord> AnalysisManager::takeNextGame() {
    if (reader_) {
        return readPlayableGame(*reader_, gamesRead_, false);
    }
    if (nextIndex_ >= games_.size()) {
        return std::nullopt;
    }
    return games_[nextIndex_];
}

void AnalysisManager::schedule(const std::shared_ptr<AnalysisManager>& self, const EngineConfig& engine,
    GameManagerPool& pool) {
    pool.addTaskProvider(self, engine);
    pool.startManagers();
}

std::optional<GameTask> AnalysisManager::nextTask() {
    std::scoped_lock lock(mutex_);
    auto game = takeNextGame();
    if (!game) {
        return std::nullopt;
    }

    const auto index = nextIndex_++;
    playersInFlight_[index] = { game->getWhiteEngineName(), game->getBlackEngineName() };

    GameTask task;
    task.taskId = std::to_string(index);
    task.taskType = direction_ == AnalysisDirection::Backward
        ? GameTask::Type::ReplayBackward
        : GameTask::Type::ReplayForward;
    task.gameRecord = std::move(*game);
    task.gameRecord.setTimeControl(timeControl_, timeControl_);
    // The game is numbered as it stands in the file, from the start rather than only when it
    // comes back: whoever watches the run needs to know which game they are looking at.
    task.gameRecord.setTotalGameNo(static_cast<uint32_t>(index) + 1);
    // Everything an earlier search said about the moves goes, so the walk can be seen while it
    // happens: a move carrying an evaluation is one this run has already been through, and one
    // without is a move still ahead of it. The players are kept aside, so they can be put back
    // when the game comes home.
    for (auto& move : task.gameRecord.history()) {
        move.clearSearchInfo();
    }
    return task;
}

void AnalysisManager::setGameRecord(const std::string& taskId, const GameRecord& record) {
    const auto index = QaplaHelpers::to_uint32(taskId);
    if (!index) {
        return;
    }

    GameRecord analysed = record;
    {
        std::scoped_lock lock(mutex_);
        const auto players = playersInFlight_.find(*index);
        if (players == playersInFlight_.end()) {
            return;
        }
        // Starting a game replaces the players with the engines playing it, which is right for a
        // game and wrong for an analysis: the players are part of what is being analysed. The
        // engine that produced the evaluations is not a player at all, it goes into the tag PGN
        // has for exactly that - without it, two engines analysing the same file would write two
        // indistinguishable copies of every game.
        analysed.setWhiteEngineName(players->second.first);
        analysed.setBlackEngineName(players->second.second);
        analysed.setTag("Annotator", engineName_);
        playersInFlight_.erase(players);
    }
    // The games are numbered as they stand in the file; a replayed game brings no number of its
    // own, and every game would be written as round 0.
    analysed.setTotalGameNo(*index + 1);

    // A replay ends with the game rewound, and a game that is not at its last move does not count
    // as finished - which is what the PGN writer asks before it saves anything.
    analysed.setNextMoveIndex(static_cast<uint32_t>(analysed.history().size()));
    pgnSink().saveGame(analysed);

    finishedCount_++;
    logGameResult(*index, analysed);
}

void AnalysisManager::logGameResult(size_t index, const GameRecord& record) const {
    const auto& history = record.history();
    const auto evaluated = std::ranges::count_if(history, [](const MoveRecord& move) {
        return move.scoreCp.has_value() || move.scoreMate.has_value();
    });

    // The first move is the one a backward walk ends on, and the one the whole exercise is about:
    // its evaluation is the one that already knows how the game went.
    std::string firstMoveScore = "n/a";
    if (!history.empty() && (history.front().scoreCp || history.front().scoreMate)) {
        firstMoveScore = history.front().evalString();
    }

    Logger::reportLogger().log(
        std::format("analysis game {} moves {} evaluated {} first move {} score {}",
            index + 1, history.size(), evaluated,
            history.empty() ? "-" : history.front().san_, firstMoveScore),
        TraceLevel::result);
}

} // namespace QaplaTester
