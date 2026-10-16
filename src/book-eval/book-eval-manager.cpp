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
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include "book-eval-manager.h"

#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"
#include "../base-elements/string-helper.h"
#include "../chess-game/game-record.h"

#include <filesystem>
#include <format>

namespace QaplaTester {

namespace {

constexpr uint32_t PROGRESS_INTERVAL_MS = 10000;

std::string formatDuration(double seconds) {
    const auto total = static_cast<uint64_t>(seconds);
    return std::format("{}:{:02}:{:02}", total / 3600, (total / 60) % 60, total % 60);
}

} // namespace

std::string BookEvalManager::describeSearch(const EngineConfig& engine) {
    return std::format("engine={} tc={}", engine.getName(), engine.getTimeControl().toPgnTimeControlString());
}

size_t BookEvalManager::initialize(const BookEvalConfig& config, const EngineConfig& engine) {
    BookTree tree;
    try {
        tree = BookTree::read(config.bookFile);
    } catch (const std::exception& error) {
        throw AppError::makeInvalidParameters(std::format("Give a 9 bit book as bookeval.book: {}", error.what()));
    }

    const auto search = describeSearch(engine);
    if (std::filesystem::exists(config.outFile)) {
        BookTree evaluated;
        try {
            evaluated = BookTree::read(config.outFile);
        } catch (const std::exception& error) {
            throw AppError::makeInvalidParameters(std::format(
                "bookeval.out exists and is no book to continue from: {}", error.what()));
        }
        if (evaluated.header().payloadId != BOOK_SCORE_PAYLOAD_ID || !evaluated.sameTree(tree)) {
            throw AppError::makeInvalidParameters(std::format(
                "bookeval.out '{}' is not the evaluation of '{}'; remove it or name another file.",
                config.outFile, config.bookFile));
        }
        if (evaluated.header().version != search) {
            throw AppError::makeInvalidParameters(std::format(
                "bookeval.out '{}' was evaluated with '{}', this run searches with '{}'; continue it "
                "with the same engine and limit, or name another file.",
                config.outFile, evaluated.header().version, search));
        }
        tree = std::move(evaluated);
    }

    std::scoped_lock lock(mutex_);
    tree_ = std::move(tree);
    config_ = config;
    timeControl_ = engine.getTimeControl();
    outHeader_ = QaplaBook::BookHeader{};
    outHeader_.author = "qapla-engine-tester --bookeval";
    outHeader_.name = tree_.header().name;
    // The version record names the search, so that a run is only ever continued with the same one.
    outHeader_.version = search;

    open_.clear();
    inFlight_.clear();
    nextOpen_ = 0;
    positionCount_ = 0;
    evaluatedCount_ = 0;
    searchedInRun_ = 0;
    failedCount_ = 0;
    for (uint32_t position = 0; position < tree_.positionCount(); ++position) {
        if (!tree_.isPlayedFrom(position)) {
            continue;
        }
        ++positionCount_;
        if (tree_.score(position) == BookScore::NONE) {
            open_.push_back(position);
        } else {
            ++evaluatedCount_;
        }
    }
    startTime_ = std::chrono::steady_clock::now();
    saveTimer_ = QaplaHelpers::CallbackTimer(config.saveIntervalMs, [this] {
        tree_.writeWithScores(config_.outFile, outHeader_);
    });
    progressTimer_ = QaplaHelpers::CallbackTimer(PROGRESS_INTERVAL_MS, [this] { logProgressLocked(); });
    return open_.size();
}

void BookEvalManager::schedule(const std::shared_ptr<BookEvalManager>& self, const EngineConfig& engine,
    GameManagerPool& pool) {
    pool.addTaskProvider(self, engine);
    pool.startManagers();
}

std::optional<GameTask> BookEvalManager::nextTask() {
    std::scoped_lock lock(mutex_);
    if (nextOpen_ >= open_.size()) {
        return std::nullopt;
    }
    if (config_.maxPositions != 0 && nextOpen_ >= config_.maxPositions) {
        return std::nullopt;
    }
    const auto position = open_[nextOpen_++];

    GameTask task;
    task.taskType = GameTask::Type::ComputeMove;
    task.taskId = std::to_string(position);
    const auto line = tree_.lineTo(position);
    for (const auto& lan : line) {
        MoveRecord move;
        move.original = lan;
        move.lan_ = lan;
        task.gameRecord.addMove(move);
    }
    task.gameRecord.setTimeControl(timeControl_, timeControl_);
    task.gameRecord.setPositionName(std::format("book position {}", position));
    inFlight_[position] = line.size();
    return task;
}

void BookEvalManager::setGameRecord(const std::string& taskId, const GameRecord& record) {
    const auto position = QaplaHelpers::to_uint32(taskId);
    if (!position) {
        return;
    }
    std::scoped_lock lock(mutex_);
    const auto flight = inFlight_.find(*position);
    if (flight == inFlight_.end()) {
        return;
    }
    const auto lineLength = flight->second;
    inFlight_.erase(flight);

    const auto& moves = record.history();
    const bool searched = moves.size() == lineLength + 1
        && (moves.back().scoreCp.has_value() || moves.back().scoreMate.has_value());
    if (!searched) {
        ++failedCount_;
        Logger::reportLogger().log(std::format(
            "bookeval: the search of book position {} brought no score, it stays open", *position),
            TraceLevel::error);
        return;
    }
    tree_.setScore(*position, BookScore::fromSearch(moves.back().scoreCp, moves.back().scoreMate));
    ++evaluatedCount_;
    ++searchedInRun_;
    saveTimer_.update();
    progressTimer_.update();
}

void BookEvalManager::save() {
    std::scoped_lock lock(mutex_);
    tree_.writeWithScores(config_.outFile, outHeader_);
}

size_t BookEvalManager::getPositionCount() const {
    std::scoped_lock lock(mutex_);
    return positionCount_;
}

size_t BookEvalManager::getEvaluatedCount() const {
    std::scoped_lock lock(mutex_);
    return evaluatedCount_;
}

size_t BookEvalManager::getFailedCount() const {
    std::scoped_lock lock(mutex_);
    return failedCount_;
}

bool BookEvalManager::isComplete() const {
    std::scoped_lock lock(mutex_);
    return evaluatedCount_ == positionCount_;
}

double BookEvalManager::getPositionsPerSecond() const {
    std::scoped_lock lock(mutex_);
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - startTime_;
    return elapsed.count() > 0 ? static_cast<double>(searchedInRun_) / elapsed.count() : 0.0;
}

bool BookEvalManager::isRunFinished() const {
    std::scoped_lock lock(mutex_);
    const bool nothingLeft = nextOpen_ >= open_.size()
        || (config_.maxPositions != 0 && nextOpen_ >= config_.maxPositions);
    return nothingLeft && inFlight_.empty();
}

void BookEvalManager::logProgress() const {
    std::scoped_lock lock(mutex_);
    logProgressLocked();
}

void BookEvalManager::logProgressLocked() const {
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - startTime_;
    const double rate = elapsed.count() > 0 ? static_cast<double>(searchedInRun_) / elapsed.count() : 0.0;
    const auto open = positionCount_ - evaluatedCount_;
    const double share = positionCount_ == 0 ? 100.0
        : 100.0 * static_cast<double>(evaluatedCount_) / static_cast<double>(positionCount_);
    Logger::reportLogger().log(std::format(
        "bookeval {} of {} positions ({:.1f}%), {:.1f} positions/s, {} open, eta {}",
        evaluatedCount_, positionCount_, share, rate, open,
        rate > 0 ? formatDuration(static_cast<double>(open) / rate) : std::string("-")),
        TraceLevel::result);
}

} // namespace QaplaTester
