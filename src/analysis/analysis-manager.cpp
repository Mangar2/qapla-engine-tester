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

} // namespace

size_t AnalysisManager::initialize(const AnalysisConfig& config) {
    PgnIO reader;
    PgnIO::LoadParams params;
    params.filePath = config.pgnFile;
    params.loadComments = true;
    params.skipEmptyGames = true;
    if (config.maxGames != 0) {
        params.maxGames = config.maxGames;
    }

    auto result = reader.loadGamesWithResult(params);
    if (!result.fileOpened) {
        throw AppError::makeInvalidParameters(std::format(
            "Give a readable PGN file as analysis.pgn; '{}' could not be opened.", config.pgnFile));
    }
    if (result.errorCount != 0) {
        Logger::reportLogger().log(
            std::format("{} of {} games in '{}' could not be read", result.errorCount,
                result.getTotalCount(), config.pgnFile),
            TraceLevel::warning);
    }

    direction_ = config.direction;

    std::scoped_lock lock(mutex_);
    games_.clear();
    games_.reserve(result.games.size());
    for (size_t i = 0; i < result.games.size(); ++i) {
        // A game read from a PGN carries its moves in SAN, and the engine is sent the moves in
        // LAN: replaying the game once fills both, the way a tournament does it with its
        // openings. A game whose moves do not all play out comes back short and is left out
        // rather than analysed up to its broken move.
        GameState state;
        GameRecord game = state.setFromGameRecordAndCopy(result.games[i], std::nullopt, false);
        // The replay fills the notations but leaves the two fields a game played here would
        // carry: the parsed move, and the move as the mover sent it - which is what replaying a
        // game forward hands back to the players. A game read from a PGN has neither.
        if (!fillPlayedMoveFields(game)) {
            Logger::reportLogger().log(
                std::format("game {} of '{}' has a move that cannot be played, it is left out",
                    i + 1, config.pgnFile),
                TraceLevel::warning);
            continue;
        }
        if (game.history().empty() || game.history().size() != result.games[i].history().size()) {
            Logger::reportLogger().log(
                std::format("game {} of '{}' has a move that cannot be played, it is left out",
                    i + 1, config.pgnFile),
                TraceLevel::warning);
            continue;
        }
        games_.push_back(std::move(game));
    }
    return games_.size();
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
}

void AnalysisManager::schedule(const std::shared_ptr<AnalysisManager>& self, const EngineConfig& engine,
    GameManagerPool& pool) {
    pool.addTaskProvider(self, engine);
    pool.startManagers();
}

std::optional<GameTask> AnalysisManager::nextTask() {
    std::scoped_lock lock(mutex_);
    if (nextIndex_ >= games_.size()) {
        return std::nullopt;
    }

    const auto index = nextIndex_++;

    GameTask task;
    task.taskId = std::to_string(index);
    task.taskType = direction_ == AnalysisDirection::Backward
        ? GameTask::Type::ReplayBackward
        : GameTask::Type::ReplayForward;
    task.gameRecord = games_[index];
    task.gameRecord.setTimeControl(timeControl_, timeControl_);
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
        if (*index >= games_.size()) {
            return;
        }
        // Starting a game replaces the players with the engines playing it, which is right for a
        // game and wrong for an analysis: the players are part of what is being analysed. The
        // engine that produced the evaluations is not a player at all, it goes into the tag PGN
        // has for exactly that - without it, two engines analysing the same file would write two
        // indistinguishable copies of every game.
        const auto& original = games_[*index];
        analysed.setWhiteEngineName(original.getWhiteEngineName());
        analysed.setBlackEngineName(original.getBlackEngineName());
        analysed.setTag("Annotator", engineName_);
    }
    // The games are numbered as they stand in the file; a replayed game brings no number of its
    // own, and every game would be written as round 0.
    analysed.setTotalGameNo(*index + 1);

    // A replay ends with the game rewound, and a game that is not at its last move does not count
    // as finished - which is what the PGN writer asks before it saves anything.
    analysed.setNextMoveIndex(static_cast<uint32_t>(analysed.history().size()));
    PgnSave::tournament().saveGame(analysed);

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
