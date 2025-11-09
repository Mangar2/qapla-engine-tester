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
#include "string-helper.h"

namespace QaplaTester {

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
				std::cout << "\n\nQuit received, finishing all games and analyses before exiting.\n\n" << std::flush;
                this->setConcurrency(0, true);
			} else if (cmd == InputHandler::ImmediateCommand::Abort) {
				std::cout << "\n\nAbort received, terminating all ongoing games and analyses immediately.\n\n" << std::flush;
			 	this->stopAll();
            } else if (cmd == InputHandler::ImmediateCommand::Concurrency) {
                updateConcurrency(value);
            } else if (cmd == InputHandler::ImmediateCommand::Running) {
                this->printRunningGames(std::cout);
            } else if (cmd == InputHandler::ImmediateCommand::ViewGame) {
                this->viewEngineTrace(value ? std::stoi(*value) : 0);
            } else if (cmd == InputHandler::ImmediateCommand::Pause) {
                if (paused_) {
					std::cout << "\n\nResuming.\n\n" << std::flush;
                } else {
					std::cout << "\n\nPausing. All current tasks will finish before pause takes effect.\n\n" << std::flush;
                }
                this->togglePause();
			}
        });
}

void GameManagerPool::updateConcurrency(InputHandler::CommandValue &value)
{
    if (value)
    {
        try
        {
            auto concurrency = QaplaHelpers::to_int(*value);
            if (concurrency && *concurrency >= 0)
            {
                this->setConcurrency(static_cast<uint32_t>(*concurrency), true, true);
                std::cout << "\n\nSetting concurrency to " << *value << "\n\n"
                          << std::flush;
            }
            else
            {
                throw std::invalid_argument("Negative concurrency value");
            }
        }
        catch (...)
        {
            std::cout << "\n\nInvalid concurrency value: " << *value
                      << ". Please provide a non-negative whole number.\n\n"
                      << std::flush;
        }
    }
}

void GameManagerPool::withGameRecords(
    const std::function<void(const GameRecord&, uint32_t)>& accessFn,
    const std::function<bool(uint32_t)>& filterFn
) {
    std::scoped_lock lock(managerMutex_); 
    uint32_t gameIndex = 0;
    for (auto& gameManager : managers_) {
        if (filterFn(gameIndex) && gameManager->isRunning()) {
            gameManager->withGameRecord([&](const GameRecord& record) {
                accessFn(record, gameIndex);
            });
        }
        gameIndex++;
    }
}

void GameManagerPool::withEngineRecords(
    const std::function<void(const EngineRecords&, uint32_t)>& accessFn,
    const std::function<bool(uint32_t)>& filterFn
) {
    std::scoped_lock lock(managerMutex_);
    uint32_t gameIndex = 0;
    for (auto& gameManager : managers_) {
        if (filterFn(gameIndex) && gameManager->isRunning()) {
            // Check if the access function is interested before calculating EngineRecords
            gameManager->getGameContext().withEngineRecords([&](const EngineRecords& records) {
                accessFn(records, gameIndex);
            });
        }
        gameIndex++;
    }
}

void GameManagerPool::withMoveRecord(
    const std::function<void(const MoveRecord&, uint32_t, uint32_t)>& accessFn,
    const std::function<bool(uint32_t)>& filterFn
) {
    std::scoped_lock lock(managerMutex_);
    uint32_t gameIndex = 0;
    for (auto& gameManager : managers_) {
        if (filterFn(gameIndex) && gameManager && gameManager->isRunning()) {
            gameManager->getGameContext().withMoveRecord([&](const MoveRecord& record, uint32_t playerIndex) {
                accessFn(record, gameIndex, playerIndex);
            });
        }
        gameIndex++;
    }
}

void GameManagerPool::printRunningGames(std::ostream& out) const {
    out << "\n\nCurrently running games:\n";
    int pos = 1;
    for (const auto& managerPtr : managers_) {
        GameManager* manager = managerPtr.get();
        if (!manager->isRunning()) {
            continue;
        }
        auto* whiteEngine = manager->getEngine(true);
		auto whiteName = whiteEngine != nullptr ? whiteEngine->getConfig().getName() : "";
		auto* blackEngine = manager->getEngine(false);
		auto blackName = blackEngine != nullptr ? blackEngine->getConfig().getName() : "";
        out << std::setw(2) << pos << ". "
              << std::left << std::setw(30) << whiteName
              << " vs "
              << std::left << std::setw(30) << blackName
			  << (manager->isPaused() ? "[PAUSED]" : "[RUNNING]")
              << "\n";
        ++pos;
    }
    std::cout << "\n" << std::flush;
}

size_t GameManagerPool::runningGameCount() const {
    size_t count = 0;
    for (const auto& managerPtr : managers_) {
        GameManager* manager = managerPtr.get();
        if (manager->isRunning()) {
            ++count;
        }
    }
    return count;
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

void GameManagerPool::addTaskProvider(std::shared_ptr<GameTaskProvider> taskProvider, 
    const EngineConfig& engineName) 
{
    TaskAssignment task;
    task.provider = std::move(taskProvider);
    task.engine1 = engineName;
    {
        std::scoped_lock lock(taskMutex_);
        taskAssignments_.push_back(std::move(task));
    }
}

void GameManagerPool::addTaskProvider(std::shared_ptr<GameTaskProvider> taskProvider,
    const EngineConfig& whiteEngine, const EngineConfig& blackEngine) 
{
    TaskAssignment task;
    task.provider = std::move(taskProvider);
    task.engine1 = whiteEngine;
    task.engine2 = blackEngine;
    {
        std::scoped_lock lock(taskMutex_);
        taskAssignments_.push_back(std::move(task));
    }
}

void GameManagerPool::setConcurrency(uint32_t count, bool nice, bool start) 
{
    if (count == maxConcurrency_ && !start) {
        return;
    }
    maxConcurrency_ = count;
    niceMode_ = nice;
    ensureManagerCount(maxConcurrency_);
    if (start) {
        startManagers();
    }
}

void GameManagerPool::stopAll() {
    std::scoped_lock lockManager(managerMutex_);
    std::scoped_lock lockTask(taskMutex_);
    for (auto& manager : managers_) {
        manager->stop();
    }
}

void GameManagerPool::clearAll() {
    stopAll();
    std::scoped_lock lockManager(managerMutex_);
    for (auto& manager : managers_) {
        const auto& future = manager->getFinishedFuture();
        if (future.valid()) {
            future.wait();
        }
    }

    std::scoped_lock lock(taskMutex_);
    taskAssignments_.clear();
}

void GameManagerPool::togglePause() {
    std::scoped_lock lock(taskMutex_);
    for (auto& manager : managers_) {
        if (paused_) {
            manager->resume();
        } else {
            manager->pause();
		}
    }
    paused_ = !paused_;
}

void GameManagerPool::waitForTaskPolling(std::chrono::milliseconds pollingIntervalMs) {
    while (true) {
        {
            std::scoped_lock lock(managerMutex_);
            bool allIdle = true;
            for (const auto& managerPtr : managers_) {
                GameManager* manager = managerPtr.get();
                const auto& future = manager->getFinishedFuture();
                if (future.valid() &&
                    future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) 
                {
                    allIdle = false;
                    break;
                }
            }
            if (allIdle) {
                break;
            }
        }
        std::this_thread::sleep_for(pollingIntervalMs);
    }

    std::scoped_lock lock(taskMutex_);
    taskAssignments_.clear();
}

void GameManagerPool::waitForTask() {

    while (true) {
        std::scoped_lock lock(managerMutex_);
        std::vector<GameManager*> managers;
        {
            for (const auto& managerPtr : managers_) {
                GameManager* manager = managerPtr.get();
                const auto& future = manager->getFinishedFuture();
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
            const auto& future = manager->getFinishedFuture();
            if (future.valid()) {
                future.wait();
            }
        }
    }

    std::scoped_lock lock(taskMutex_);
    taskAssignments_.clear();
}

bool GameManagerPool::areAllTasksFinished() {
    std::scoped_lock lock(managerMutex_);
    return std::ranges::all_of(managers_, [](const auto& managerPtr) {
        GameManager* manager = managerPtr.get();
        const auto& future = manager->getFinishedFuture();
        return (!future.valid() || future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    });
}

void GameManagerPool::startManagers() {
    auto toStart = maxConcurrency_;
    std::scoped_lock lock(startManagerMutex_);
    {
        std::scoped_lock lock(managerMutex_);
        auto runningCnt = countActiveManagers();
        if (maxConcurrency_ <= runningCnt) {
            return;
        }
        toStart -= runningCnt;
    }
    for (size_t i = 0; i < managers_.size() && toStart > 0; ++i) {
        GameManager* manager;
        {
            std::scoped_lock lock(taskMutex_);
            manager = managers_[i].get();
        }
		if (manager != nullptr && !manager->isRunning()) {
			manager->start();
            toStart--;
		}
	}
}

void GameManagerPool::ensureManagerCount(size_t count) {
    std::scoped_lock lock(managerMutex_);

    size_t current = managers_.size();
    if (count <= current) {
        return;
    }

    for (size_t i = current; i < count; ++i) {
        auto newManager = std::make_unique<GameManager>(this);
        {
            std::scoped_lock lock(taskMutex_);
            managers_.push_back(std::move(newManager));
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
        if (manager->isRunning()) {
            ++count;
        }
    }
    return count;
}

std::optional<GameManager::ExtendedTask> GameManagerPool::tryAssignNewTask() {
    std::scoped_lock lock(taskMutex_);

    for (auto& assignment : taskAssignments_) {
        if (!assignment.engine1) {
            continue;
        }
        if (!assignment.provider) {
            continue;
        }

        auto taskOpt = assignment.provider->nextTask();
        if (!taskOpt.has_value()) {
            continue;
        }

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
    std::scoped_lock lock(managerMutex_);
	if (taskProvider == nullptr) {
		return false;
    }
	bool tooMany = countActiveManagers() > maxConcurrency_;
	if (tooMany) {
		taskProvider.reset();
	}
    return tooMany;
}

} // namespace QaplaTester
