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

void GameManagerPool::addTask(GameTaskProvider* taskProvider, const std::string& engineName) {
    std::lock_guard lock(mutex_);

    Task task;
    task.provider = taskProvider;
    task.engine1 = engineName;
    task.concurrency = maxConcurrency_;

    ensureManagerCount(maxConcurrency_);
    assignTaskToManagers(task);

    tasks_.push_back(std::move(task));
}

void GameManagerPool::addTask(GameTaskProvider* taskProvider, const std::string& whiteEngine, const std::string& blackEngine) {
    std::lock_guard lock(mutex_);

    Task task;
    task.provider = taskProvider;
    task.engine1 = whiteEngine;
    task.engine2 = blackEngine;
    task.concurrency = maxConcurrency_;

    ensureManagerCount(maxConcurrency_);
    assignTaskToManagers(task);

    tasks_.push_back(std::move(task));
}

void GameManagerPool::setConcurrency(size_t count, bool nice) {
    std::lock_guard lock(mutex_);
    maxConcurrency_ = count;
    niceMode_ = nice;
}

void GameManagerPool::clearAll() {
    std::lock_guard lock(mutex_);
    for (auto& manager : managers_) {
        manager->stop();
    }
    managers_.clear();
}

void GameManagerPool::waitForTask(GameTaskProvider* taskProvider) {
    std::vector<GameManager*> managers;
    {
        std::lock_guard lock(mutex_);
        auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const Task& task) {
            return task.provider == taskProvider;
            });
        if (it == tasks_.end()) {
            return;
        }

        for (GameManager* manager : it->managers) {
            managers.push_back(manager);
        }
    }

    for (auto& manager : managers) {
        manager->getFinishedFuture().wait();
    }

    std::lock_guard lock(mutex_);
    std::erase_if(tasks_, [&](const Task& task) {
        return task.provider == taskProvider;
        });
}

void GameManagerPool::ensureManagerCount(size_t count) {
    size_t current = managers_.size();
    if (count <= current) return;

    for (size_t i = current; i < count; ++i) {
        managers_.push_back(std::make_unique<GameManager>());
    }
}

void GameManagerPool::assignTaskToManagers(Task& task) {
    if (task.engine2.has_value()) {
        auto whiteEngines = EngineWorkerFactory::createEnginesByName(task.engine1, task.concurrency);
        auto blackEngines = EngineWorkerFactory::createEnginesByName(*task.engine2, task.concurrency);

        for (size_t i = 0; i < task.concurrency; ++i) {
            GameManager* manager = managers_[i].get();
            manager->setEngines(std::move(whiteEngines[i]), std::move(blackEngines[i]));
            manager->computeTasks(task.provider);
            task.managers.push_back(manager);
        }
    }
    else {
        auto engines = EngineWorkerFactory::createEnginesByName(task.engine1, task.concurrency);

        for (size_t i = 0; i < task.concurrency; ++i) {
            GameManager* manager = managers_[i].get();
            manager->setUniqueEngine(std::move(engines[i]));
            manager->computeTasks(task.provider);
            task.managers.push_back(manager);
        }
    }
}