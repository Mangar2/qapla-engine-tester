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

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <variant>

#include "engine-option.h"


/**
 * Stores configuration for a chess engine including its executable path,
 * working directory and available options. Supports loading and saving from INI files.
 */
class EngineConfig {
public:
    using Value = std::variant<std::string, int, bool, float>;
    using ValueMap = std::unordered_map<std::string, Value>;
    /**
     * @brief Creates a fully initialized EngineConfig instance from a value map.
     * @param values A map of parameters for an engine.
     * @return Fully constructed and validated EngineConfig.
     * @throws std::runtime_error if required fields are missing or invalid.
     */
    static EngineConfig createFromValueMap(const ValueMap& values) {
		EngineConfig config;
		config.setCommandLineOptions(values);
		return config;
    }
    static EngineConfig createFromPath(const std::string executablePath) {
		EngineConfig config;
        config.setExecutablePath(executablePath);
        config.finalizeSetOptions();
		return config;
    }
    /**
    * 
     * Sets the name of the engine.
     * @param engineName The name to assign.
     */
    void setName(const std::string& engineName) { name = engineName; }

    /**
     * Sets the path to the engine executable.
     * @param path The executable path.
     */
    void setExecutablePath(const std::string& path) { 
        executablePath = path; 
    }

    /**
     * Sets the working directory for the engine.
     * @param path The working directory path.
     */
    void setWorkingDirectory(const std::string& path) { workingDirectory = path; }

	/**
	 * Sets the protocol used by the engine.
	 * @param proto The protocol to set (Uci, XBoard, etc.).
	 */
	void setProtocol(EngineProtocol proto) { protocol = proto; }

    /**
     * Gets the engine name.
     * @return The engine name.
     */
    const std::string& getName() const { return name; }

    /**
     * Gets the path to the engine executable.
     * @return The executable path.
     */
    const std::string& getExecutablePath() const { return executablePath; }

    /**
     * Gets the working directory.
     * @return The working directory path.
     */
    const std::string& getWorkingDirectory() const { return workingDirectory; }

	/**
	 * Gets the protocol used by the engine.
	 * @return The engine protocol.
	 */
	EngineProtocol getProtocol() const { return protocol; }

    /**
     * Gets the current option values.
     * @return A map of option names to their values.
     */
    const std::unordered_map<std::string, std::string>& getOptionValues() const { return optionValues; }

    /**
     * Sets a specific option value 
     * @param key The option name.
     * @param value The value to assign.
     */
    void setOptionValue(const std::string& key, const std::string& value) {
        optionValues[key] = value;
    }

    /**
     * @brief Sets multiple options at once from a map of key-value pairs coming from the command line
     * @param values A map of option names and their values.
     * @throw std::runtime_error 
     *  - if a key is encountered twice 
     *  - if an unknown key is encountered.
	 *  - if a required key is missing.
     */
    void setCommandLineOptions(const ValueMap& values, bool update = false);

    /**
	 * @brief Filters the current options based on the available options.
	 * @param availableOptions The set of options that are available for the engine.
	 * @return A map of option names and their values that are present in the available options.
     */
    std::unordered_map<std::string, std::string> getOptions(const EngineOptions availableOptions) const;

    friend std::istream& operator>>(std::istream& in, EngineConfig& config);

    friend std::ostream& operator<<(std::ostream& out, const EngineConfig& config);


private:

    /**
     * @brief Reads the section header line from the input stream and sets the engine name.
     * @param in The input stream positioned at a section header line.
     * @throws std::runtime_error If the line is not a valid section header.
     */
    void readHeader(std::istream& in);
    void finalizeSetOptions();


    std::string to_string(const Value& value);
    std::string name;
    std::string executablePath;
    std::string workingDirectory;
    EngineProtocol protocol = EngineProtocol::Unknown;
    std::unordered_map<std::string, std::string> optionValues;
};

