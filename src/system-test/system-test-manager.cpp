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

#include "system-test-manager.h"

#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iostream>

namespace QaplaTester {

namespace {
[[nodiscard]] uint64_t toKnpsRounded(double nps) {
    if (nps <= 0.0) {
        return 0;
    }
    return static_cast<uint64_t>(std::llround(nps / 1000.0));
}
}

void SystemTestManager::initialize(const EngineConfig& engine, const SystemTestConfig& config) {
    if (config.maxCores == 0) {
        throw AppError::makeInvalidParameters("systemtest.maxcores must be at least 1.");
    }
    if (config.step == 0) {
        throw AppError::makeInvalidParameters("systemtest.step must be at least 1.");
    }
    if (config.stepTimeSeconds == 0) {
        throw AppError::makeInvalidParameters("systemtest.steptime must be at least 1 second.");
    }

    engine_ = engine;
    config_ = config;
    finished_.store(false);
    replayReady_.store(false);
    nextTaskId_.store(1);
    currentConcurrency_.store(0);
    initialConcurrency_.store(1);

    {
        std::scoped_lock lock(stateMutex_);
        baselineStandardDeviation_.reset();
        replayRecord_.reset();
        seedPlyCount_ = 0;
        resetAggregate();
    }
}

void SystemTestManager::schedule(const std::shared_ptr<SystemTestManager>& self, GameManagerPool& pool) {
    poolController_ = pool.getController();
    const auto startConcurrency = std::min(config_.maxCores, config_.step);
    initialConcurrency_.store(startConcurrency);
    currentConcurrency_.store(1);
    stepStartTime_ = std::chrono::steady_clock::now();
    stepDeadline_ = stepStartTime_ + std::chrono::seconds(config_.stepTimeSeconds);

    Logger::reportLogger().logStatus(
        std::format(
            "systemtest start engine {} tc {} maxcores {} step {} steptime {}s (seed game at 1 core)",
            engine_.getName(),
            engine_.getTimeControl().toPgnTimeControlString(),
            config_.maxCores,
            config_.step,
            config_.stepTimeSeconds),
        "systemtest",
        TraceLevel::result);

    pool.setConcurrency(1, true);
    pool.addTaskProvider(self, engine_, engine_);
    pool.startManagers();
}

std::optional<GameTask> SystemTestManager::nextTask() {
    if (finished_.load()) {
        return std::nullopt;
    }
    const auto taskNumber = nextTaskId_.fetch_add(1);
    return createTask(taskNumber);
}

void SystemTestManager::setGameRecord([[maybe_unused]] const std::string& taskId, const GameRecord& record) {
    if (storeReplaySeedIfNeeded(record)) {
        startReplayPhaseAfterSeed();
        return;
    }

    {
        std::scoped_lock lock(stateMutex_);
        stepAggregate_.games += 1;

        const auto& history = record.history();
        if (seedPlyCount_ != 0 && history.size() != seedPlyCount_) {
            Logger::reportLogger().log(
                std::format("FATAL: Game length mismatch. Expected {} moves, found {}.", seedPlyCount_, history.size()),
                TraceLevel::error);
        }

        for (size_t ply = 0; ply < history.size(); ++ply) {
            accumulatePlyStats(ply, history[ply]);
        }
    }
    
    updateStepIfRequired();
}

void SystemTestManager::accumulatePlyStats(size_t ply, const MoveRecord& move) {
    if (move.nodes == 0 || move.timeMs == 0) {
        return;
    }

    const auto nps = (static_cast<double>(move.nodes) * 1000.0) / static_cast<double>(move.timeMs);
    if (ply >= stepAggregate_.plies.size()) {
        stepAggregate_.plies.resize(ply + 1);
    }
    auto& stats = stepAggregate_.plies[ply];
    stats.count++;
    stats.sumNps += nps;
    stats.sumNpsSquared += nps * nps;

    stepAggregate_.samples++;
    stepAggregate_.totalNodes += move.nodes;

    if (stats.count == 1) {
        stats.firstDepth = move.depth;
        stats.firstMove = move.lan_;
        return;
    }

    if (stats.firstMove != move.lan_ && !stats.moveMismatchLogged) {
        Logger::reportLogger().log(
            std::format("FATAL: Move mismatch at ply {}. Expected {}, found {}.", ply, stats.firstMove, move.lan_),
            TraceLevel::error);
        stats.moveMismatchLogged = true;
    }
    if (config_.test && stats.firstDepth != move.depth && !stats.depthMismatchLogged) {
        std::cout << std::format("INFO: Depth mismatch at ply {}. Expected {}, found {}.\n", ply, stats.firstDepth, move.depth);
        stats.depthMismatchLogged = true;
    }
}

GameTask SystemTestManager::createTask(uint64_t taskNumber) const {
    GameTask task;
    task.taskId = std::format("systemtest-{}", taskNumber);
    if (replayReady_.load(std::memory_order_acquire)) {
        task.taskType = GameTask::Type::ReplayForward;
        task.gameRecord = replayRecord_.value();
        task.gameRecord.setPositionName("systemtest-replay");
        task.gameRecord.setWhiteEngineName(engine_.getName());
        task.gameRecord.setBlackEngineName(engine_.getName());
        task.gameRecord.setTimeControl(engine_.getTimeControl(), engine_.getTimeControl());
        return task;
    }

    task.taskType = GameTask::Type::PlayGame;
    task.gameRecord.setPositionName("systemtest-seed");
    task.gameRecord.setStartPosition(true, "", true, 0, engine_.getName(), engine_.getName());
    task.gameRecord.setTimeControl(engine_.getTimeControl(), engine_.getTimeControl());
    return task;
}

bool SystemTestManager::storeReplaySeedIfNeeded(const GameRecord& record) {
    if (replayReady_.load(std::memory_order_acquire)) {
        return false;
    }
    std::scoped_lock lock(stateMutex_);
    if (replayRecord_.has_value()) {
        return false;
    }
    if (record.history().empty()) {
        return false;
    }

    replayRecord_ = record.createMinimalCopy();
    seedPlyCount_ = record.history().size();
    baselineStandardDeviation_.reset();
    resetAggregate();
    replayReady_.store(true, std::memory_order_release);
    return true;
}

void SystemTestManager::startReplayPhaseAfterSeed() {
    const auto targetConcurrency = initialConcurrency_.load();
    currentConcurrency_.store(targetConcurrency);
    stepStartTime_ = std::chrono::steady_clock::now();
    stepDeadline_ = stepStartTime_ + std::chrono::seconds(config_.stepTimeSeconds);

    if (poolController_ != nullptr) {
        poolController_->setConcurrency(targetConcurrency, true, true);
    }

    Logger::reportLogger().logStatus(
        std::format("systemtest replay seed captured; switching to replay mode at concurrency {}", targetConcurrency),
        "systemtest",
        TraceLevel::result);
}

void SystemTestManager::updateStepIfRequired() {
    if (finished_.load()) {
        return;
    }
    if (std::chrono::steady_clock::now() < stepDeadline_) {
        return;
    }
    completeCurrentStepAndAdvance();
}

void SystemTestManager::completeCurrentStepAndAdvance() {
    const auto concurrency = currentConcurrency_.load();
    const auto stepEndTime = std::chrono::steady_clock::now();

    StepResult result;
    double baselineStdDev = 0.0;
    bool isFinished = false;
    uint32_t nextConcurrency = 0;

    {
        std::scoped_lock lock(stateMutex_);
        if (stepEndTime < stepDeadline_) {
            return;
        }

        const auto elapsedMs = static_cast<uint64_t>(std::max<int64_t>(1,
            std::chrono::duration_cast<std::chrono::milliseconds>(stepEndTime - stepStartTime_).count()));

        result = buildStepResult(concurrency, stepAggregate_, elapsedMs);
        resetAggregate();

        if (!baselineStandardDeviation_.has_value() && result.samples > 0) {
            baselineStandardDeviation_ = result.averageStandardDeviation;
        }
        baselineStdDev = baselineStandardDeviation_.value_or(0.0);

        isFinished = concurrency >= config_.maxCores;
        if (isFinished) {
            finished_.store(true);
        } else {
            nextConcurrency = std::min(config_.maxCores, concurrency + config_.step);
            currentConcurrency_.store(nextConcurrency);
            stepStartTime_ = std::chrono::steady_clock::now();
            stepDeadline_ = stepStartTime_ + std::chrono::seconds(config_.stepTimeSeconds);
        }
    }

    logStepResult(result, baselineStdDev);

    if (isFinished) {
        if (poolController_ != nullptr) {
            poolController_->stopAll();
        }
        Logger::reportLogger().logStatus("systemtest completed", "systemtest", TraceLevel::result);
    } else {
        if (poolController_ != nullptr) {
            poolController_->setConcurrency(nextConcurrency, true, true);
        }
        Logger::reportLogger().logStatus(
            std::format("systemtest next step concurrency {}", nextConcurrency),
            "systemtest",
            TraceLevel::result);
    }
}

SystemTestManager::StepResult SystemTestManager::buildStepResult(uint32_t concurrency, const StepAggregate& aggregate, uint64_t stepElapsedMs) {
    StepResult result;
    result.concurrency = concurrency;
    result.games = aggregate.games;
    result.samples = aggregate.samples;
    result.stepElapsedMs = stepElapsedMs;

    if (stepElapsedMs > 0) {
        result.totalNps = (static_cast<double>(aggregate.totalNodes) * 1000.0)
            / static_cast<double>(stepElapsedMs);
    }

    if (aggregate.samples > 0) {
        double sumAverages = 0.0;
        double sumStandardDeviations = 0.0;
        uint64_t validPliesForAvg = 0;

        for (const auto& stats : aggregate.plies) {
            if (stats.count > 0) {
                double mean = stats.sumNps / static_cast<double>(stats.count);
                sumAverages += mean;
                
                double variance = 0.0;
                if (stats.count > 1) {
                    variance = (stats.sumNpsSquared - static_cast<double>(stats.count) * mean * mean) / static_cast<double>(stats.count - 1);
                    variance = std::max(0.0, variance); // Guard against negative variance due to floating-point errors
                }
                sumStandardDeviations += std::sqrt(variance);
                validPliesForAvg++;
            }
        }

        if (validPliesForAvg > 0) {
            result.averageNps = sumAverages / static_cast<double>(validPliesForAvg);
            result.averageStandardDeviation = sumStandardDeviations / static_cast<double>(validPliesForAvg);
        }
    }
    return result;
}

void SystemTestManager::logStepResult(const StepResult& result, double baselineStandardDeviation) {
    uint32_t relativeStandardDeviation = 100;
    if (baselineStandardDeviation > 0.0) {
        relativeStandardDeviation = static_cast<uint32_t>(std::round((result.averageStandardDeviation / baselineStandardDeviation) * 100.0));
    }

    Logger::reportLogger().logStatus(
        std::format(
            "systemtest step cores {} games {} samples {} total-nps {}knps avg-nps {}knps stddev {}knps basis-stddev {}knps relative-stddev {}%",
            result.concurrency,
            result.games,
            result.samples,
            toKnpsRounded(result.totalNps),
            toKnpsRounded(result.averageNps),
            toKnpsRounded(result.averageStandardDeviation),
            toKnpsRounded(baselineStandardDeviation),
            relativeStandardDeviation),
        "systemtest",
        TraceLevel::result);
}

void SystemTestManager::resetAggregate() {
    stepAggregate_ = StepAggregate{};
    stepAggregate_.plies.resize(seedPlyCount_);
}

} // namespace QaplaTester
