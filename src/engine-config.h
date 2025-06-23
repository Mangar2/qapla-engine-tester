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

#include "time-control.h"
#include "engine-option.h"
#include "logger.h"


/**
 * Stores configuration for a chess engine including its executable path,
 * working directory and available options. Supports loading and saving from INI files.
 */
class EngineConfig {
public:
    EngineConfig() = default;
    EngineConfig(const EngineConfig&) = default;

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
    void setName(const std::string& engineName) { name_ = engineName; }

    /**
     * Sets the path to the engine executable.
     * @param path The executable path.
     */
    void setExecutablePath(const std::string& path) { 
        executablePath_ = path; 
    }

    /**
     * Sets the working directory for the engine.
     * @param path The working directory path.
     */
    void setWorkingDirectory(const std::string& path) { workingDirectory_ = path; }

	/**
	 * Sets the protocol used by the engine.
	 * @param proto The protocol to set (Uci, XBoard, etc.).
	 */
	void setProtocol(EngineProtocol proto) { protocol_ = proto; }

    /**
     * Gets the engine name.
     * @return The engine name.
     */
    const std::string& getName() const { return name_; }

    /**
     * Gets the path to the engine executable.
     * @return The executable path.
     */
    const std::string& getExecutablePath() const { return executablePath_; }

    /**
     * Gets the working directory.
     * @return The working directory path.
     */
    const std::string& getWorkingDirectory() const { return workingDirectory_; }

	/**
	 * Gets the protocol used by the engine.
	 * @return The engine protocol.
	 */
	EngineProtocol getProtocol() const { return protocol_; }

	void setPonder(bool enabled) { ponder_ = enabled; }
	bool isPonderEnabled() const { return ponder_; }

	void setGauntlet(bool enabled) { gauntlet_ = enabled; }
	bool isGauntlet() const { return gauntlet_; }

    void setTimeControl(const std::string& tc);
	const TimeControl& getTimeControl() const { return tc_; }

    void setTraceLevel(const std::string& level);
	TraceLevel getTraceLevel() const { return traceLevel_; }

    /**
     * Gets the current option values.
     * @return A map of option names to their values.
     */
    const std::unordered_map<std::string, std::string>& getOptionValues() const { return optionValues_; }

    /**
     * Sets a specific option value 
     * @param key The option name.
     * @param value The value to assign.
     */
    void setOptionValue(const std::string& key, const std::string& value) {
        optionValues_[key] = value;
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
     * @brief Returns a map of disambiguation-relevant parameters for external ID generation.
     * Includes only key fields that distinguish configurations, excluding path or working directory.
     * @return Map of key-value string pairs relevant for ID disambiguation.
     */
    std::unordered_map<std::string, std::string> toDisambiguationMap() const;


    /**
	 * @brief Filters the current options based on the available options.
	 * @param availableOptions The set of options that are available for the engine.
	 * @return A map of option names and their values that are present in the available options.
     */
    std::unordered_map<std::string, std::string> getOptions(const EngineOptions availableOptions) const;

    friend std::istream& operator>>(std::istream& in, EngineConfig& config);

    friend std::ostream& operator<<(std::ostream& out, const EngineConfig& config);

    /**
     * @brief Compares two EngineConfig instances for equality.
     *        All members except traceLevel_ are considered.
     * @param lhs First EngineConfig.
     * @param rhs Second EngineConfig.
     * @return True if all relevant configuration fields match.
     */
    friend bool operator==(const EngineConfig& lhs, const EngineConfig& rhs);

private:

    /**
     * @brief Reads the section header line from the input stream and sets the engine name.
     * @param in The input stream positioned at a section header line.
     * @throws std::runtime_error If the line is not a valid section header.
     */
    void readHeader(std::istream& in);
    void finalizeSetOptions();

    std::string toString(const Value& value);
    std::string name_;
    std::string executablePath_;
    std::string workingDirectory_;
    TimeControl tc_;
	TraceLevel traceLevel_ = TraceLevel::command;
    EngineProtocol protocol_ = EngineProtocol::Unknown;
    bool ponder_ = false;
	bool gauntlet_ = false;
    std::unordered_map<std::string, std::string> optionValues_;
};

