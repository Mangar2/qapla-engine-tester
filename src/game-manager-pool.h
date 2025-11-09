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

#include "engine-config.h"
#include "game-manager.h"
#include "input-handler.h"

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <ostream>

namespace QaplaTester {

class GameTaskProvider;

/**
 * @brief Manages a pool of GameManager instances and distributes tasks based on concurrency.
 */
class GameManagerPool {
public:
    struct GameInfo {
        std::string white;
        std::string black;
        std::string status;
    };
    GameManagerPool();

    /**
     * @brief Adds a new task with one engine per manager.
     *
     * @param taskProvider Task source
     * @param engine Engine configuration
     */
    void addTaskProvider(std::shared_ptr<GameTaskProvider> taskProvider, const EngineConfig& engine);

    /**
     * @brief Adds a new task with two engines per manager.
     *
     * @param taskProvider Task source
     * @param whiteEngine Configuration of the white engine
     * @param blackEngine Configuration of the black engine
     */
    void addTaskProvider(std::shared_ptr<GameTaskProvider> taskProvider, const EngineConfig& whiteEngine,
        const EngineConfig& blackEngine);

    /**
     * @brief Starts additional game managers to reach the maximum concurrency.
     *
     * This method checks the number of currently active game managers and starts
     * additional managers if the number is less than the `maxConcurrency_` value.
     * It iterates through the pool of managers and starts inactive ones (those
     * without a task provider) until the desired concurrency level is reached.
     *
     * @note This method does not create new managers; it only activates existing ones.
     */
    void startManagers();

    /**
     * @brief Sets the global concurrency limit.
     *
     * @param count Maximum number of concurrent managers
     * @param providerType Type of game task provider
     * @param nice If true, idle managers are reduced gradually
	 * @param start If true, starts the managers immediately
     */
    void setConcurrency(uint32_t count, bool nice = true, bool start = false);

    /**
     * @brief Attempts to assign a new task to a game manager.
     *
     * This method iterates through the task assignments and tries to retrieve the next
     * available task from a task provider. If a task is available, it creates the necessary
     * engine workers (white and black) and returns the task along with its associated data.
     *
     * @return An optional `GameManager::ExtendedTask` containing the task and its associated
     *         engines and provider, or `std::nullopt` if no task is available.
     *
     * @throws AppError If no engine configuration is provided for the task assignment.
     * @throws std::runtime_error If engine creation fails for the task assignment.
     */
    std::optional<GameManager::ExtendedTask> tryAssignNewTask();

    /**
     * @brief Stops all managers and clears all resources.
     */
    void stopAll();

    /**
     * @brief Clears all task assignments and stops all managers.
	 */
    void clearAll();

    void togglePause();

    /**
	 * @brief Blocks until all managers have completed all available tasks.
     */
    void waitForTask();

    /**
     * @brief Blocks until all tasks have finished, polling the status periodically.
     * @param pollingIntervalMs The interval in milliseconds to wait between status checks.
     */
    void waitForTaskPolling(std::chrono::milliseconds pollingIntervalMs = std::chrono::milliseconds(100));

    /**
     * @brief Checks if all tasks have finished without blocking.
     * @return True if all futures are ready, false otherwise.
     */
    bool areAllTasksFinished();

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
	 * @brief Deactivates a GameManager if we have too many running managers.
	 * To be thread safe, this is done here by setting taskProvider to nullptr
     * 
	 * @param taskProvider Reference to the GameTaskProvider pointer to clear.
	 * @return True if the manager was deactivated, false otherwise.
	 */
	bool maybeDeactivateManager(std::shared_ptr<GameTaskProvider>& taskProvider);

    /**
     * @brief Returns the number of currently running games.
     *
     * @return The count of active GameManager instances.
	 */
    [[nodiscard]] size_t runningGameCount() const;

    /**
     * @brief Executes a provided function on the GameRecord of all active GameManagers in a thread-safe manner.
     *
     * This method iterates over all active GameManagers in the pool and calls their
     * `withGameRecord` method, passing the provided function. The entire iteration
     * is protected by a mutex to ensure thread safety.
     *
     * @param accessFn A function to be executed on the GameRecord of each active GameManager.
     * @param filterFn A function to filter the GameRecords based on their index.
     */
    void withGameRecords(
        const std::function<void(const GameRecord&, uint32_t)>& accessFn,
        const std::function<bool(uint32_t)>& filterFn
    );

    /**
     * @brief Executes the given callable with thread-safe access to the engine records of all players.
     * @param accessFn A callable that takes a const EngineRecords&.
     * @param filterFn A callable that takes a uint32_t and returns a bool, used to filter currently viewed games.
     */
    void withEngineRecords(
        const std::function<void(const EngineRecords&, uint32_t)>& accessFn,
        const std::function<bool(uint32_t)>& filterFn
    );

    /**
     * @brief Executes the given callable with thread-safe access to the move records of all players.
     * @param accessFn A callable that takes a const MoveRecord&.
     * @param filterFn A callable that takes a uint32_t and returns a bool, used to filter currently viewed games.
     */
    void withMoveRecord(
        const std::function<void(const MoveRecord&, uint32_t, uint32_t)>& accessFn,
        const std::function<bool(uint32_t)>& filterFn
    );

private:
    
    void printRunningGames(std::ostream& out) const;
    void viewEngineTrace(int gameManagerIndex) const;
    void updateConcurrency(InputHandler::CommandValue &value);

    /**
     * @brief Returns the singleton instance of the GameManagerPool.
     */
    static std::unique_ptr<GameManagerPool>& getInstanceUniquePtr() {
        static std::unique_ptr<GameManagerPool> instance = std::make_unique<GameManagerPool>();
        return instance;
    }

    struct TaskAssignment {
        std::shared_ptr<GameTaskProvider> provider;
        std::optional<EngineConfig> engine1;
        std::optional<EngineConfig> engine2;
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
    [[nodiscard]] uint32_t countActiveManagers() const;

    /**
     * @brief Ensures that there are at least count managers.
     *
     * This function adjusts the pool of game managers to ensure that the total number
     * of active managers is equal to the given `count`. If there are fewer managers
     * than required, new ones are created. 
     *
     * @param count The desired number of game managers.
     */
    void ensureManagerCount(size_t count);

    std::vector<TaskAssignment> taskAssignments_;
    std::vector<std::unique_ptr<GameManager>> managers_;
    uint32_t maxConcurrency_ = 0;
    bool niceMode_ = false;
    std::mutex taskMutex_;
    std::mutex managerMutex_;
    std::mutex startManagerMutex_;
    bool paused_ = false;

	// InputHandler
    std::unique_ptr<InputHandler::CallbackRegistration> inputCallback_;
};

} // namespace QaplaTester
