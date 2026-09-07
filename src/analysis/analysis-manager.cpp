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

#include "reverse-analysis.h"

#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"
#include "../base-elements/string-helper.h"
#include "../game-manager/game-state.h"
#include "../opening/pgn-io.h"

#include <algorithm>
#include <format>

namespace QaplaTester {

size_t ReverseAnalysis::initialize(const ReverseAnalysisConfig& config) {
    if (config.moveTimeMs == 0) {
        throw AppError::makeInvalidParameters("reverse.movetime must be at least 1 millisecond.");
    }

    PgnIO reader;
    PgnIO::LoadParams params;
    params.filePath = config.file;
    params.skipEmptyGames = true;
    if (config.maxGames != 0) {
        params.maxGames = config.maxGames;
    }

    auto result = reader.loadGamesWithResult(params);
    if (!result.fileOpened) {
        throw AppError::makeInvalidParameters(std::format("Could not open PGN file '{}'.", config.file));
    }
    if (result.errorCount != 0) {
        Logger::reportLogger().log(
            std::format("{} of {} games in '{}' could not be read", result.errorCount,
                result.getTotalCount(), config.file),
            TraceLevel::warning);
    }

    timeControl_ = TimeControl();
    timeControl_.setMoveTime(config.moveTimeMs);
    fileName_ = config.file;

    std::scoped_lock lock(mutex_);
    games_.clear();
    games_.reserve(result.games.size());
    for (size_t i = 0; i < result.games.size(); ++i) {
        // A game read from a PGN carries its moves in SAN, and the engine is sent the moves in
        // LAN: replaying the game once fills both, and a game whose moves do not all play out
        // comes back short and is left out rather than analysed up to its broken move.
        GameState state;
        GameRecord game = state.setFromGameRecordAndCopy(result.games[i], std::nullopt, false);
        if (game.history().empty() || game.history().size() != result.games[i].history().size()) {
            Logger::reportLogger().log(
                std::format("game {} of '{}' has a move that cannot be played, it is left out",
                    i + 1, config.file),
                TraceLevel::warning);
            continue;
        }
        games_.push_back(std::move(game));
    }
    analysed_.assign(games_.size(), false);
    nextIndex_ = 0;
    finishedCount_ = 0;
    return games_.size();
}

void ReverseAnalysis::schedule(const std::shared_ptr<ReverseAnalysis>& self, const EngineConfig& engine,
    GameManagerPool& pool) {
    pool.addTaskProvider(self, engine);
    pool.startManagers();
}

std::optional<GameTask> ReverseAnalysis::nextTask() {
    std::scoped_lock lock(mutex_);
    if (nextIndex_ >= games_.size()) {
        return std::nullopt;
    }

    const auto index = nextIndex_++;

    GameTask task;
    task.taskId = std::to_string(index);
    task.taskType = GameTask::Type::ReplayBackward;
    task.gameRecord = games_[index];
    task.gameRecord.setTimeControl(timeControl_, timeControl_);
    return task;
}

void ReverseAnalysis::setGameRecord(const std::string& taskId, const GameRecord& record) {
    const auto index = QaplaHelpers::to_uint32(taskId);
    if (!index) {
        return;
    }

    {
        std::scoped_lock lock(mutex_);
        if (*index >= games_.size()) {
            return;
        }
        games_[*index] = record;
        analysed_[*index] = true;
    }

    finishedCount_++;
    logGameResult(*index, record);
}

std::vector<GameRecord> ReverseAnalysis::getAnalysedGames() const {
    std::scoped_lock lock(mutex_);
    std::vector<GameRecord> analysed;
    for (size_t i = 0; i < games_.size(); ++i) {
        if (analysed_[i]) {
            analysed.push_back(games_[i]);
        }
    }
    return analysed;
}

void ReverseAnalysis::logGameResult(size_t index, const GameRecord& record) const {
    const auto& history = record.history();
    const auto evaluated = std::ranges::count_if(history, [](const MoveRecord& move) {
        return move.scoreCp.has_value() || move.scoreMate.has_value();
    });

    // The first move is the one the backward walk ends on, and the one the whole exercise is
    // about: its evaluation is the one that already knows how the game went.
    std::string firstMoveScore = "n/a";
    if (!history.empty() && (history.front().scoreCp || history.front().scoreMate)) {
        firstMoveScore = history.front().evalString();
    }

    Logger::reportLogger().log(
        std::format("reverse game {} moves {} evaluated {} first move {} score {}",
            index + 1, history.size(), evaluated,
            history.empty() ? "-" : history.front().san_, firstMoveScore),
        TraceLevel::result);
}

} // namespace QaplaTester
