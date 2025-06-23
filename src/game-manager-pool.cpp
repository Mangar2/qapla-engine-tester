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

#include "game-manager.h"
#include "game-manager-pool.h"
#include "engine-worker-factory.h"

void GameManagerPool::addTaskProvider(GameTaskProvider* taskProvider, const EngineConfig& engineName, int maxManagers) {
    std::lock_guard lock(mutex_);

    TaskAssignment task;
    task.provider = taskProvider;
    task.engine1 = engineName;
    task.maxManagers = maxManagers;

    assignTaskToManagers(task);

    taskAssignments_.push_back(std::move(task));
}

void GameManagerPool::addTaskProvider(GameTaskProvider* taskProvider, 
    const EngineConfig& whiteEngine, const EngineConfig& blackEngine, int maxManagers) {
    std::lock_guard lock(mutex_);

    TaskAssignment task;
    task.provider = taskProvider;
    task.engine1 = whiteEngine;
    task.engine2 = blackEngine;
    task.maxManagers = maxManagers;
    taskAssignments_.push_back(std::move(task));
    assignTaskToManagers(task);
}

void GameManagerPool::setConcurrency(int count, bool nice) {
    std::lock_guard lock(mutex_);
    maxConcurrency_ = count;
    niceMode_ = nice;
    ensureManagerCount(maxConcurrency_);
}

void GameManagerPool::clearAll() {
    std::lock_guard lock(mutex_);
    for (auto& manager : managers_) {
        manager->stop();
    }
    managers_.clear();
}

void GameManagerPool::waitForTask() {
    std::vector<GameManager*> managers;
    {
        std::lock_guard lock(mutex_);
        for (const auto& managerPtr : managers_) {
            GameManager* manager = managerPtr.get();
            if (manager->getTaskProvider() != nullptr) {
                managers.push_back(manager);
            }
        }
    }

    for (auto& manager : managers) {
        manager->getFinishedFuture().wait();
    }

    std::lock_guard lock(mutex_);
    taskAssignments_.clear();
}

void GameManagerPool::ensureManagerCount(size_t count) {
    size_t current = managers_.size();
    if (count <= current) return;

    for (size_t i = current; i < count; ++i) {
        managers_.push_back(std::make_unique<GameManager>());
    }
}

std::vector<GameManager*> GameManagerPool::collectAvailableManagers() {
    std::vector<GameManager*> available;
    for (const auto& managerPtr : managers_) {
        GameManager* manager = managerPtr.get();
        if (manager->getTaskProvider() == nullptr) {
            available.push_back(manager);
        }
    }
    return available;
}

void GameManagerPool::assignTaskToManagers(TaskAssignment& task) {
	auto availableManagers = collectAvailableManagers();
	auto assignCount = std::min(task.maxManagers, availableManagers.size());
	if (assignCount == 0) {
		return;
	}

    if (task.engine1 && task.engine2) {
        auto whiteEngines = EngineWorkerFactory::createEngines(*task.engine1, assignCount);
        auto blackEngines = EngineWorkerFactory::createEngines(*task.engine2, assignCount);

        for (size_t i = 0; i < assignCount; ++i) {
            GameManager* manager = availableManagers[i];
            manager->setEngines(std::move(whiteEngines[i]), std::move(blackEngines[i]));
            manager->computeTasks(task.provider);
        }
    }
    else if (task.engine1) {
        auto engines = EngineWorkerFactory::createEngines(*task.engine1, assignCount);

        for (size_t i = 0; i < assignCount; ++i) {
            GameManager* manager = availableManagers[i];
            manager->setUniqueEngine(std::move(engines[i]));
            manager->computeTasks(task.provider);
        }
    }
}

std::optional<GameManager::ExtendedTask> GameManagerPool::tryAssignNewTask() {
    std::lock_guard lock(mutex_);

    for (auto& assignment : taskAssignments_) {
        if (!assignment.engine1) continue;
        if (!assignment.provider) continue;

        auto taskOpt = assignment.provider->nextTask(
            assignment.engine1->getName(), 
            assignment.engine2 ? assignment.engine2->getName(): assignment.engine1->getName());
        if (!taskOpt.has_value())
            continue;

        GameManager::ExtendedTask result;
        result.task = std::move(taskOpt.value());
        result.provider = assignment.provider;

        if (assignment.engine1 && assignment.engine2) {
            auto whiteEngines = EngineWorkerFactory::createEngines(*assignment.engine1, 1);
            auto blackEngines = EngineWorkerFactory::createEngines(*assignment.engine2, 1);

            result.white = std::move(whiteEngines.front());
            result.black = std::move(blackEngines.front());
        }
        else if (assignment.engine1) {
            auto engines = EngineWorkerFactory::createEngines(*assignment.engine1, 1);
            result.white = std::move(engines.front());
        }

        return result;
    }

    return std::nullopt;
}

