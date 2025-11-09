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
 * @copyright Copyright (c) 2021 Volker Böhm
 * @Overview
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include "engine-config.h"
#include "cli-settings-manager.h"

namespace QaplaTester {

class EngineConfigManager {
public:

    /**
     * @brief Loads engine configurations from a file.
     *        Only valid EngineConfig entries are added; parsing stops at the first [section] header.
     * @param filePath Path to the tournament or config file.
     * @throws std::runtime_error if the file cannot be opened.
     */
    void loadFromFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Unable to open file: " + filePath);
        }
        loadFromStream(file);
    }

    /**
     * @brief Loads engine configurations from a stream.
     *        Stops parsing at the first non-config section (e.g., [round...]).
     * @param in The input stream containing engine configuration lines.
     */
    void loadFromStream(std::istream& in);

    /**
     * Saves all configurations to an INI file.
     * Each configuration is separated by a blank line.
     * @param filePath Path to the INI file.
     */
    void saveToFile(const std::string& filePath) const;
    
    /**
	 * @brief Saves all configurations to an output stream.
	 * @param out The output stream to write the configurations to.
     */
    void saveToStream(std::ostream& out) const;

    /**
     * Returns all engine configurations.
     * @return A vector of EngineConfig.
     */
    [[nodiscard]] std::vector<EngineConfig> getAllConfigs() const {
        return configs;
    }

    /**
     * Retrieves a configuration by engine name.
     * @param name The name of the engine.
     * @return A pointer to the EngineConfig or nullptr if not found.
     */
    [[nodiscard]] const EngineConfig* getConfig(const std::string& name) const;

    /**
     * Retrieves a mutable configuration by engine name.
     * @param name The name of the engine.
     * @return A pointer to the EngineConfig or nullptr if not found.
     */
    EngineConfig* getConfigMutable(const std::string& name);

    /**
     * Retrieves a mutable configuration by command and protocol.
     * @param cmd The command.
     * @param proto The protocol.
     * @return A pointer to the EngineConfig or nullptr if not found.
     */
    EngineConfig* getConfigMutableByCmdAndProtocol(const std::string& cmd, EngineProtocol proto);

    /**
     * Sets the configuration at the specified index.
     * @param index The index of the configuration to set.
     * @param config The new EngineConfig to set.
     */
    void setConfig(uint32_t index, const EngineConfig& config) {
        if (index < configs.size()) {
            configs[index] = config;
        }
    }

    /**
     * Adds a new configuration or replaces the existing one with the same name.
     * @param config The EngineConfig to add or update.
     */
    void addOrReplaceConfig(const EngineConfig& config, bool replaceOnDifferentProtocol = false);

    /**
     * Adds a new configuration without checking for duplicates.
     * @param config The EngineConfig to add.
	 */
    void addConfig(const EngineConfig& config) {
        configs.push_back(config);
	}

	/**
	 * @brief Removes a configuration by value.
     */
    void removeConfig(const EngineConfig& remove) {
        auto [first, last] = std::ranges::remove(configs, remove);
        configs.erase(first, last);
    }	/**
	 * @brief Returns a list of error messages encountered during parsing.
	 */
	[[nodiscard]] const std::vector<std::string>& getErrors() const {
		return errors;
	}

    /**
     * @brief Add or replaces several EngineConfig instances from configuration maps.
     *        Each map must represent one complete engine configuration.
	 * @param instances A collection of GroupInstances, each containing a map of configuration values.
     * @throws std::runtime_error if any EngineConfig is invalid.
     */
    void addOrReplaceConfig(const CliSettings::GroupInstances& instances) {
        for (const auto& instance : instances) {
			const auto& map = instance.getValues();
            EngineConfig config = EngineConfig::createFromValueMap(map);
            addOrReplaceConfig(config);
        }
    }

	/**
	 * @brief Add or replaces a single EngineConfig instance from a GroupInstance.
	 * @param valueMap The values for the configuration.
	 */
    void addOrReplaceConfig(const CliSettings::ValueMap& valueMap) {
		EngineConfig config = EngineConfig::createFromValueMap(valueMap);
		addOrReplaceConfig(config);
	}

    /**
     * @brief Compares loaded engine configs with a reference list and returns all matching names.
     *
     * @param reference Reference list of known engine configs.
     * @return Set of engine names that exist identically in both sets.
     */
    [[nodiscard]] std::unordered_set<std::string> findMatchingNames(const std::vector<EngineConfig>& reference) const;

    /**
     * @brief Assigns unique display names to engines with identical base names.
     *        Modifies the names of engines in-place to disambiguate between similar engines.
     *        Uses toDisambiguationMap() to determine differentiating attributes.
     *
     * @param engines Vector of EngineConfig references to modify
     */
    static void assignUniqueDisplayNames(std::vector<EngineConfig> &engines);

private:
    std::vector<EngineConfig> configs;
	std::vector<std::string> errors; // Stores error messages during parsing
};

} // namespace QaplaTester
