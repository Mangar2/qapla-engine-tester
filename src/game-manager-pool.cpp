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
#include "app-error.h"

GameManagerPool::GameManagerPool() {
    inputCallback_ = InputHandler::getInstance().registerCommandCallback(
		{ InputHandler::ImmediateCommand::Quit,
          InputHandler::ImmediateCommand::Abort,
		  InputHandler::ImmediateCommand::Concurrency,
		  InputHandler::ImmediateCommand::Pause,
          InputHandler::ImmediateCommand::Running,
          InputHandler::ImmediateCommand::ViewGame },
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
                    try {
                        int concurrency = std::stoi(*value);
                        if (concurrency >= 0) {
                            this->setConcurrency(static_cast<uint32_t>(concurrency), true, true);
                            std::cout << "\n\nSetting concurrency to " << *value << "\n" << std::endl;
                        }
                        else {
							throw std::invalid_argument("Negative concurrency value");
                        }
					}
                    catch (...) {
                        std::cout << "\n\nInvalid concurrency value: " << *value 
                            << ". Please provide a non-negative whole number.\n" << std::endl;
                    }
				}
			}
            else if (cmd == InputHandler::ImmediateCommand::Running) {
                this->printRunningGames();
            }
            else if (cmd == InputHandler::ImmediateCommand::ViewGame) {
                this->viewEngineTrace(value ? std::stoi(*value) : 0);
            } 
            else if (cmd == InputHandler::ImmediateCommand::Pause) {
                if (paused_) {
					std::cout << "\n\nResuming.\n" << std::endl;
                } 
                else {
					std::cout << "\n\nPausing. All current tasks will finish before pause takes effect.\n" << std::endl;
                }
                this->togglePause();
			}
        });
}

void GameManagerPool::printRunningGames() const {
    std::cout << "\n\nCurrently running games:\n";
    int pos = 1;
    for (const auto& managerPtr : managers_) {
        GameManager* manager = managerPtr.get();
        if (!manager->getTaskProvider()) {
            continue;
        }
		auto& whiteName = manager->getEngine(true)->getConfig().getName();
        auto& blackName = manager->getEngine(false)->getConfig().getName();
        std::cout << std::setw(2) << pos << ". "
              << std::left << std::setw(30) << whiteName
              << " vs "
              << std::left << std::setw(30) << blackName
			  << (manager->isPaused() ? "[PAUSED]" : "[RUNNING]")
              << "\n";
        ++pos;
    }
    std::cout << std::endl;
}

void GameManagerPool::viewEngineTrace(int gameManagerIndex) const {
    for (const auto& managerPtr : managers_) {
        GameManager* manager = managerPtr.get();
        if (manager->getTaskProvider() == nullptr) {
            continue;
        }
        if (gameManagerIndex == 1) {
            manager->setCliTraceLevel(TraceLevel::info);
        }
        else {
            manager->setCliTraceLevel(Logger::engineLogger().getCliThreshold());
        }
        gameManagerIndex--;
    }
}

void GameManagerPool::addTaskProvider(std::shared_ptr<GameTaskProvider> taskProvider, const EngineConfig& engineName) {
    TaskAssignment task;
    task.provider = taskProvider;
    task.engine1 = engineName;
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        taskAssignments_.push_back(std::move(task));
    }
}

void GameManagerPool::addTaskProvider(std::shared_ptr<GameTaskProvider> taskProvider,
    const EngineConfig& whiteEngine, const EngineConfig& blackEngine) 
{
    TaskAssignment task;
    task.provider = taskProvider;
    task.engine1 = whiteEngine;
    task.engine2 = blackEngine;
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        taskAssignments_.push_back(std::move(task));
    }
}

void GameManagerPool::setConcurrency(uint32_t count, bool nice, bool start) {
    std::lock_guard<std::mutex> lock(managerMutex_);
    maxConcurrency_ = count;
    niceMode_ = nice;
    if (start) {
        tryReactivateManagers();
    }
    ensureManagerCount(maxConcurrency_, start);
}

void GameManagerPool::stopAll() {
    std::lock_guard<std::mutex> lock(taskMutex_);
    for (auto& manager : managers_) {
        manager->stop();
    }
}

void GameManagerPool::togglePause() {
    std::lock_guard<std::mutex> lock(taskMutex_);
    for (auto& manager : managers_) {
        if (paused_) {
            manager->resume();
        } else {
            manager->pause();
		}
    }
    paused_ = !paused_;
}

void GameManagerPool::waitForTask() {

    while (true) {
        std::vector<GameManager*> managers;
        {
            std::lock_guard<std::mutex> lock(taskMutex_);
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

    std::lock_guard<std::mutex> lock(taskMutex_);
    taskAssignments_.clear();
}

void GameManagerPool::tryReactivateManagers() {
	for (size_t i = 0; i < managers_.size() && i < maxConcurrency_; ++i) {
        GameManager* manager;
        {
            std::lock_guard<std::mutex> lock(taskMutex_);
            manager = managers_[i].get();
        }
		if (manager && manager->getTaskProvider() == nullptr) {
			manager->start();
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
            std::lock_guard<std::mutex> lock(taskMutex_);
            managers_.push_back(std::move(newManager));
        }
        if (start) {
            rawPtr->start();
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

uint32_t GameManagerPool::countActiveManagers() const {
    uint32_t count = 0;
    for (const auto& managerPtr : managers_) {
        GameManager* manager = managerPtr.get();
        if (manager->getTaskProvider() != nullptr) {
            ++count;
        }
    }
    return count;
}

void GameManagerPool::assignTaskToManagers() {
    std::lock_guard<std::mutex> lock(managerMutex_);
	auto availableManagers = collectAvailableManagers();
    if (availableManagers.empty()) {
        return; 
	}

    for (size_t i = 0; i < availableManagers.size(); ++i) {
        GameManager* manager = availableManagers[i];
        if (!manager->start()) break;
	}
}

std::optional<GameManager::ExtendedTask> GameManagerPool::tryAssignNewTask() {
    std::lock_guard<std::mutex> lock(taskMutex_);

    for (auto& assignment : taskAssignments_) {
        if (!assignment.engine1) continue;
        if (!assignment.provider) continue;

        auto taskOpt = assignment.provider->nextTask();
        if (!taskOpt.has_value())
            continue;

        GameManager::ExtendedTask result;
        result.task = std::move(taskOpt.value());
        result.provider = assignment.provider;

        if (!assignment.engine1) {
            throw AppError::make("GameManagerPool::tryAssignNewTask; No engine configuration provided for task assignment");
        }

        if (assignment.engine1 && assignment.engine2) {
            auto whiteEngines = EngineWorkerFactory::createEngines(*assignment.engine1, 1);
            auto blackEngines = EngineWorkerFactory::createEngines(*assignment.engine2, 1);

            if (whiteEngines.empty() || blackEngines.empty()) {
                throw std::runtime_error("Failed to create engines for task assignment ");
			}

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

bool GameManagerPool::maybeDeactivateManager(std::shared_ptr<GameTaskProvider>& taskProvider) {
    std::lock_guard<std::mutex> lock(deactivateMutex_);
	if (taskProvider == nullptr)
		return false;
	bool tooMany = countActiveManagers() > maxConcurrency_;
	if (tooMany) {
		taskProvider.reset();
	}
    return tooMany;
}
