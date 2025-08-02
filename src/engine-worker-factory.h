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

using EngineList = std::vector<std::unique_ptr<EngineWorker>>;
using ActiveEngines = std::vector<EngineConfig>;
 /**
  * @brief Factory for creating EngineAdapter instances based on engine type.
  */
class EngineWorkerFactory {
public:

	static void setConfigManager(const EngineConfigManager& configManager) {
		configManager_ = configManager;
	}

	static const EngineConfigManager& getConfigManager() {
		return configManager_;
	}

	static EngineConfigManager& getConfigManagerMutable() {
		return configManager_;
	}

	static const ActiveEngines& getActiveEngines() {
		return activeEngines_;
	}

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

	static void setSuppressInfoLines(bool suppress) {
		suppressInfoLines_ = suppress;
	}

	static std::unique_ptr<EngineWorker> restart(const EngineWorker& engine);

	/**
	 * @brief Assigns unique display names to all active engine configurations.
	 * If name collisions exist, disambiguating parameters are appended.
	 */
	static void assignUniqueDisplayNames();

private:
	static std::unique_ptr<EngineWorker> createEngine(const EngineConfig& config);
	static inline uint32_t identifier_ = 0;
	static inline EngineConfigManager configManager_;
	static inline ActiveEngines activeEngines_; // List of currently active engines

	static inline bool suppressInfoLines_ = false;
};
