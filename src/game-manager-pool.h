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

#include "game-manager.h"
class GameTaskProvider;

/**
 * @brief Manages a pool of GameManager instances and distributes tasks based on concurrency.
 */
class GameManagerPool {
public:
    /**
     * @brief Adds a new task with one engine per manager.
     *
     * @param taskProvider Task source
     * @param engineName Engine name
     */
    void addTask(GameTaskProvider* taskProvider, const std::string& engineName);

    /**
     * @brief Adds a new task with two engines per manager.
     *
     * @param taskProvider Task source
     * @param whiteEngine Name of white engine
     * @param blackEngine Name of black engine
     */
    void addTask(GameTaskProvider* taskProvider, const std::string& whiteEngine, const std::string& blackEngine);

    /**
     * @brief Sets the global concurrency limit.
     *
     * @param count Maximum number of concurrent managers
     * @param nice If true, idle managers are reduced gradually
     */
    void setConcurrency(size_t count, bool nice);

    /**
     * @brief Stops all managers and clears all resources.
     */
    void clearAll();

    /**
     * @brief Blocks until all managers for the given task have completed their current task.
     * @param taskProvider The task to wait for.
     */
    void waitForTask(GameTaskProvider* taskProvider);

    /**
     * @brief Returns the singleton instance of the GameManagerPool.
     */
	static GameManagerPool& getInstance() {
		static GameManagerPool instance;
		return instance;
	}

private:

    struct Task {
        GameTaskProvider* provider = nullptr;
        std::string engine1;
        std::optional<std::string> engine2;
        size_t concurrency = 0;
        std::vector<GameManager*> managers;
    };

    void ensureManagerCount(size_t count);
    void assignTaskToManagers(Task& task);

    std::vector<Task> tasks_;
    std::vector<std::unique_ptr<GameManager>> managers_;
    size_t maxConcurrency_ = 0;
    bool niceMode_ = false;
    std::mutex mutex_;
};
