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

GameManagerPool::GameManagerPool() {
    inputCallback_ = InputHandler::getInstance().registerCommandCallback(
		{ InputHandler::ImmediateCommand::Quit,
          InputHandler::ImmediateCommand::Abort,
		  InputHandler::ImmediateCommand::Concurrency },
        [this](InputHandler::ImmediateCommand cmd, InputHandler::CommandValue value) {
			if (cmd == InputHandler::ImmediateCommand::Quit) {
				std::cout << "\n\nQuit received, finishing all games and analyses before exiting.\n" << std::endl;
                this->setConcurrency(0, true);
			}
            else if (cmd == InputHandler::ImmediateCommand::Abort) {
				std::cout << "\n\nAbort received, terminating all ongoing games and analyses immediately.\n" << std::endl;
				this->stopAll();
            } 
            else if (cmd == InputHandler::ImmediateCommand::Concurrency) {
				if (value) {
					int concurrency = std::stoi(*value);
					this->setConcurrency(concurrency, true, true);
				}
			}
        });
}

void GameManagerPool::addTaskProvider(GameTaskProvider* taskProvider, const EngineConfig& engineName, int maxManagers) {
    std::lock_guard lock(taskMutex_);

    TaskAssignment task;
    task.provider = taskProvider;
    task.engine1 = engineName;
    task.maxManagers = maxManagers;

    assignTaskToManagers(task);

    taskAssignments_.push_back(std::move(task));
}

void GameManagerPool::addTaskProvider(GameTaskProvider* taskProvider, 
    const EngineConfig& whiteEngine, const EngineConfig& blackEngine, int maxManagers) {
    std::lock_guard lock(taskMutex_);

    TaskAssignment task;
    task.provider = taskProvider;
    task.engine1 = whiteEngine;
    task.engine2 = blackEngine;
    task.maxManagers = maxManagers;
    taskAssignments_.push_back(std::move(task));
    assignTaskToManagers(task);
}

void GameManagerPool::setConcurrency(int count, bool nice, bool start) {
    std::lock_guard lock(taskAssignmentMutex_);
    maxConcurrency_ = count;
    niceMode_ = nice;
    if (start) {
        tryReactivateManagers();
    }
    ensureManagerCount(maxConcurrency_, start);
}

void GameManagerPool::stopAll() {
    std::lock_guard lock(taskMutex_);
    for (auto& manager : managers_) {
        manager->stop();
    }
}

void GameManagerPool::waitForTask() {

    while (true) {
        std::vector<GameManager*> managers;
        {
            std::lock_guard lock(taskMutex_);
            for (const auto& managerPtr : managers_) {
                GameManager* manager = managerPtr.get();
                auto& future = manager->getFinishedFuture();
                if (future.valid() &&
                    future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) 
                {
                    managers.push_back(manager);
                }
            }
        }

		if (managers.empty()) {
            break;
		}

        for (auto& manager : managers) {
            manager->getFinishedFuture().wait();
        }
    }

    std::lock_guard lock(taskMutex_);
    taskAssignments_.clear();
}

void GameManagerPool::tryReactivateManagers() {
	for (size_t i = 0; i < managers_.size() && i < maxConcurrency_; ++i) {
        GameManager* manager;
        {
            std::lock_guard lock(taskMutex_);
            manager = managers_[i].get();
        }
		if (manager && manager->getTaskProvider() == nullptr) {
			manager->computeTasks();
		}
	}
}

void GameManagerPool::ensureManagerCount(size_t count, bool start) {
    size_t current = managers_.size();
    if (count <= current) return;

    for (size_t i = current; i < count; ++i) {
        auto newManager = std::make_unique<GameManager>();
        GameManager* rawPtr = newManager.get();
        {
            std::lock_guard lock(taskMutex_);
            managers_.push_back(std::move(newManager));
        }
        if (start) {
            rawPtr->computeTasks();
        }
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

int GameManagerPool::countActiveManagers() const {
    int count = 0;
    for (const auto& managerPtr : managers_) {
        GameManager* manager = managerPtr.get();
        if (manager->getTaskProvider() != nullptr) {
            ++count;
        }
    }
    return count;
}

void GameManagerPool::assignTaskToManagers(TaskAssignment& task) {
    std::lock_guard lock(taskAssignmentMutex_);
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
            manager->initEngines(std::move(whiteEngines[i]), std::move(blackEngines[i]));
            manager->computeTasks(task.provider);
        }
    }
    else if (task.engine1) {
        auto engines = EngineWorkerFactory::createEngines(*task.engine1, assignCount);

        for (size_t i = 0; i < assignCount; ++i) {
            GameManager* manager = availableManagers[i];
            manager->initUniqueEngine(std::move(engines[i]));
            manager->computeTasks(task.provider);
        }
    }
}

std::optional<GameManager::ExtendedTask> GameManagerPool::tryAssignNewTask() {
    std::lock_guard lock(taskMutex_);

    for (auto& assignment : taskAssignments_) {
        if (!assignment.engine1) continue;
        if (!assignment.provider) continue;

        auto taskOpt = assignment.provider->nextTask();
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

bool GameManagerPool::maybeDeactivateManager(GameTaskProvider*& taskProvider) {
    std::lock_guard lock(deactivateMutex_);
	if (taskProvider == nullptr)
		return false;
	bool tooMany = countActiveManagers() > maxConcurrency_;
	if (tooMany) {
		taskProvider = nullptr;
	}
    return tooMany;
}