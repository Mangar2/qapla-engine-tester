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
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "engine-config.h"
#include "game-manager.h"
#include "input-handler.h"

class GameTaskProvider;

/**
 * @brief Manages a pool of GameManager instances and distributes tasks based on concurrency.
 */
class GameManagerPool {
public:
    GameManagerPool();

    /**
     * @brief Adds a new task with one engine per manager.
     *
     * @param taskProvider Task source
     * @param engineConfig Engine configuration
	 * @param maxManagers maximum number of managers to use for this task
     */
    void addTaskProvider(GameTaskProvider* taskProvider, const EngineConfig& engine, int maxManagers);

    /**
     * @brief Adds a new task with two engines per manager.
     *
     * @param taskProvider Task source
     * @param whiteEngine Configuration of the white engine
     * @param blackEngine Configuration of the black engine
     * @param maxManagers maximum number of managers to use for this task
     */
    void addTaskProvider(GameTaskProvider* taskProvider, const EngineConfig& whiteEngine, 
        const EngineConfig& blackEngine, int maxManagers);

    /**
     * @brief Sets the global concurrency limit.
     *
     * @param count Maximum number of concurrent managers
     * @param nice If true, idle managers are reduced gradually
     */
	void setConcurrency(int count, bool nice) {
		setConcurrency(count, nice, false);
	}

    /**
     * @brief Stops all managers and clears all resources.
     */
    void stopAll();

    /**
     * @brief Blocks until all managers for the given task have completed their current task.
	 * if taskProvider is nullptr, waits for all tasks in the pool.
     * @param taskProvider The task to wait for.
     */
    void waitForTask();

    /**
     * @brief Returns the singleton instance of the GameManagerPool.
     */
	static GameManagerPool& getInstance() {
		return *getInstanceUniquePtr();
	}

    /**
     * @brief destructs the singleton instance
     */
    static void resetInstance() {
        getInstanceUniquePtr().reset();
    }

    /**
     * @brief Attempts to assign a new task to a free GameManager.
     *
     * Iterates over all task assignments and requests a concrete GameTask.
     * If a task is available, constructs the appropriate engines and returns an ExtendedTask.
     *
     * @return An optional ExtendedTask if a task was available, otherwise std::nullopt.
     */
    std::optional<GameManager::ExtendedTask> tryAssignNewTask();


	/**
	 * @brief Deactivates a GameManager if we have too many running managers.
	 * To be thread safe, this is done here by setting taskProvider to nullptr
     * 
	 * @param taskProvider Reference to the GameTaskProvider pointer to clear.
	 * @return True if the manager was deactivated, false otherwise.
	 */
	bool maybeDeactivateManager(GameTaskProvider*& taskProvider);

private:
    void setConcurrency(int count, bool nice, bool start);

    void printRunningGames() const;
    void viewEngineTrace(int gameManagerIndex) const;

    /**
     * @brief Returns the singleton instance of the GameManagerPool.
     */
    static std::unique_ptr<GameManagerPool>& getInstanceUniquePtr() {
        static std::unique_ptr<GameManagerPool> instance = std::make_unique<GameManagerPool>();
        return instance;
    }

    struct TaskAssignment {
        GameTaskProvider* provider = nullptr;
        std::optional<EngineConfig> engine1;
        std::optional<EngineConfig> engine2;
        size_t maxManagers = 0;
    };

    /**
     * @brief Collects all GameManager instances that are currently unassigned.
     *
     * @return A vector of pointers to available GameManagers.
     */
    std::vector<GameManager*> collectAvailableManagers();

	/**
	 * @brief Counts the number of active GameManager instances.
	 *
	 * @return The number of currently active managers.
	 */
    int countActiveManagers() const;

    void tryReactivateManagers();
    void ensureManagerCount(size_t count, bool start = false);
    void assignTaskToManagers(TaskAssignment& task);

    std::vector<TaskAssignment> taskAssignments_;
    std::vector<std::unique_ptr<GameManager>> managers_;
    int maxConcurrency_ = 0;
    bool niceMode_ = false;
    std::mutex taskMutex_;
    std::mutex taskAssignmentMutex_;
    std::mutex deactivateMutex_;

	// InputHandler
    std::unique_ptr<InputHandler::CallbackRegistration> inputCallback_;
};
