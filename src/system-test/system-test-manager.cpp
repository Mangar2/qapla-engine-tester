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
#include <format>

namespace QaplaTester {

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
    nextTaskId_.store(1);
    currentConcurrency_.store(1);

    {
        std::scoped_lock lock(stateMutex_);
        baselineVariance_.reset();
        resetAggregate();
    }
}

void SystemTestManager::schedule(const std::shared_ptr<SystemTestManager>& self, GameManagerPool& pool) {
    poolController_ = pool.getController();
    currentConcurrency_.store(1);
    stepDeadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(config_.stepTimeSeconds);

    Logger::reportLogger().logStatus(
        std::format(
            "systemtest start engine {} tc {} maxcores {} step {} steptime {}s",
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
    const auto statistics = record.calculateNpsStatistics();
    if (statistics.sampleCount > 0) {
        std::scoped_lock lock(stateMutex_);
        stepAggregate_.games += 1;
        stepAggregate_.samples += statistics.sampleCount;
        stepAggregate_.weightedNpsSum += statistics.averageNps * static_cast<double>(statistics.sampleCount);
        stepAggregate_.weightedVarianceSum += statistics.varianceNps * static_cast<double>(statistics.sampleCount);
    }
    updateStepIfRequired();
}

GameTask SystemTestManager::createTask(uint64_t taskNumber) const {
    GameTask task;
    task.taskType = GameTask::Type::PlayGame;
    task.taskId = std::format("systemtest-{}", taskNumber);
    task.gameRecord.setPositionName("systemtest");
    task.gameRecord.setStartPosition(true, "", true, 0, engine_.getName(), engine_.getName());
    task.gameRecord.setTimeControl(engine_.getTimeControl(), engine_.getTimeControl());
    return task;
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
    const auto currentConcurrency = currentConcurrency_.load();

    StepAggregate aggregate;
    {
        std::scoped_lock lock(stateMutex_);
        aggregate = stepAggregate_;
        resetAggregate();
    }
    const auto result = buildStepResult(currentConcurrency, aggregate);
    logStepResult(result);

    if (currentConcurrency >= config_.maxCores) {
        finished_.store(true);
        if (poolController_) {
            poolController_->stopAll();
        }
        Logger::reportLogger().logStatus("systemtest completed", "systemtest", TraceLevel::result);
        return;
    }

    const auto nextConcurrency = std::min(config_.maxCores, currentConcurrency + config_.step);
    currentConcurrency_.store(nextConcurrency);
    stepDeadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(config_.stepTimeSeconds);

    if (poolController_) {
        poolController_->setConcurrency(nextConcurrency, true, true);
    }

    Logger::reportLogger().logStatus(
        std::format("systemtest next step concurrency {}", nextConcurrency),
        "systemtest",
        TraceLevel::result);
}

SystemTestManager::StepResult SystemTestManager::buildStepResult(uint32_t concurrency, const StepAggregate& aggregate) {
    StepResult result;
    result.concurrency = concurrency;
    result.games = aggregate.games;
    result.samples = aggregate.samples;
    if (aggregate.samples > 0) {
        result.averageNps = aggregate.weightedNpsSum / static_cast<double>(aggregate.samples);
        result.averageVariance = aggregate.weightedVarianceSum / static_cast<double>(aggregate.samples);
    }
    return result;
}

void SystemTestManager::logStepResult(const StepResult& result) {
    double baselineVariance = 0.0;
    double additionalVariance = 0.0;
    {
        std::scoped_lock lock(stateMutex_);
        if (!baselineVariance_.has_value() && result.samples > 0) {
            baselineVariance_ = result.averageVariance;
        }
        baselineVariance = baselineVariance_.value_or(0.0);
    }
    additionalVariance = std::max(0.0, result.averageVariance - baselineVariance);

    Logger::reportLogger().logStatus(
        std::format(
            "systemtest step cores {} games {} samples {} avg-nps {:.2f} variance {:.2f} basis-varianz {:.2f} parallel-varianz {:.2f}",
            result.concurrency,
            result.games,
            result.samples,
            result.averageNps,
            result.averageVariance,
            baselineVariance,
            additionalVariance),
        "systemtest",
        TraceLevel::result);
}

void SystemTestManager::resetAggregate() {
    stepAggregate_ = StepAggregate{};
}

} // namespace QaplaTester
