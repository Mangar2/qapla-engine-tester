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
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include "engine-worker.h"
#include "engine-config-manager.h"
#include "engine-report.h"

namespace QaplaTester {

using EngineList = std::vector<std::unique_ptr<EngineWorker>>;
using ActiveEngines = std::vector<EngineConfig>;
 /**
  * @brief Factory for creating EngineAdapter instances based on engine type.
  */
class EngineWorkerFactory {
public:

	/**
	 * @brief Sets the engine configuration manager.
	 * @param configManager The EngineConfigManager to set.
	 */
	static void setConfigManager(const EngineConfigManager& configManager) {
		configManager_ = configManager;
	}

	/**
	 * @brief Retrieves the engine configuration manager.
	 * @return A constant reference to the EngineConfigManager.
	 */
	static const EngineConfigManager& getConfigManager() {
		return configManager_;
	}

	/**
	 * @brief Retrieves a mutable reference to the engine configuration manager.
	 * @return A reference to the EngineConfigManager.
	 */
	static EngineConfigManager& getConfigManagerMutable() {
		return configManager_;
	}

	/**
	 * @brief Retrieves the list of active engine configurations.
	 * @return A constant reference to the vector of active engine configurations.
	 */
	static const ActiveEngines& getActiveEngines() {
		return activeEngines_;
	}

	/**
	 * @brief Retrieves a mutable reference to the list of active engines.
	 * @return A reference to the vector of active engine configurations.
	 */
	static ActiveEngines& getActiveEnginesMutable() {
		return activeEngines_;
	}

	/**
	 * @brief Creates a list of EngineWorker instances based on the engine name.
	 * @param config The engine configuration.
	 * @param count The number of workers to create. Defaults to 1.
	 * @return A vector of unique pointers to EngineWorker instances.
	 */
	static EngineList createEngines(const EngineConfig& config, std::size_t count = 1);

	/**
	 * @brief Creates a list of EngineWorker instances, one for each configuration in the provided vector.
	 * @param configs Vector of engine configurations to create workers for.
	 * @param noWait If true, the engines are created without waiting for their startup to complete.
	 * @return A vector of unique pointers to EngineWorker instances.
	 */
	static EngineList createEngines(const std::vector<EngineConfig>& configs, bool noWait = false);

	/**
	 * @brief Sets whether to suppress info lines from the engine output.
	 * @param suppress True to suppress info lines, false to allow them.
	 */
	static void setSuppressInfoLines(bool suppress) {
		suppressInfoLines_ = suppress;
	}

	/**
	 * @brief Restarts an existing engine worker by creating a new one with the same configuration.
	 * @param worker The existing EngineWorker to restart.
	 * @return A unique pointer to the newly created EngineWorker.
	 */
	static std::unique_ptr<EngineWorker> restart(const EngineWorker& worker);

	/**
	 * @brief Assigns unique display names to all active engine configurations.
	 * If name collisions exist, disambiguating parameters are appended.
	 */
	static void assignUniqueDisplayNames();

private:
	/**
	 * @brief Creates an EngineWorker instance based on the provided engine configuration.
	 * @param config The engine configuration.
	 * @return A unique pointer to the created EngineWorker.
	 */
	static std::unique_ptr<EngineWorker> createEngine(const EngineConfig& config);

	/**
	 * @brief Waits for all newly created engines to complete their startup.
	 * @param futures The futures to wait for.
	 * @param config The engine configuration.
	 */
	static void waitUntilAllEnginesStarted(std::vector<std::future<void>>& futures, const EngineConfig& config);

	/**
	 * @brief Waits for all newly created engines to complete their startup, using config names for checklists.
	 * @param futures The futures to wait for.
	 * @param configs The engine configs corresponding to the futures.
	 */
	static void waitUntilAllEnginesStarted(std::vector<std::future<void>>& futures, const std::vector<EngineConfig>& configs);

	static inline uint32_t identifier_ = 0; ///> Unique identifier for engine workers
	static inline EngineConfigManager configManager_; ///> Engine configuration manager
	static inline ActiveEngines activeEngines_; ///> List of currently active engines

	static inline bool suppressInfoLines_ = false; ///> Flag to ignore any info lines from engines (more performance)
};

} // namespace QaplaTester
