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
#include <set>

#include "ini-file.h"
#include "string-helper.h"
#include "time-control.h"
#include "engine-option.h"
#include "logger.h"

namespace QaplaTester {

/**
 * Stores configuration for a chess engine including its executable path,
 * working directory and available options. Supports loading and saving from INI files.
 */
class EngineConfig {
public:
    EngineConfig() = default;
    EngineConfig(const EngineConfig&) = default;
	EngineConfig& operator=(const EngineConfig&) = default;

    using Value = std::variant<std::string, int, unsigned int, bool, double>;
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
    /**
     * @brief Creates an EngineConfig instance from an executable path.
     * @param executablePath The path to the engine executable.
     * @return EngineConfig instance with the executable path set.
     */
    static EngineConfig createFromPath(const std::string& executablePath) {
		EngineConfig config;
        config.setCmd(executablePath);
        config.finalizeSetOptions();
		return config;
    }

    /**
     * @brief Sets the name of the engine.
     * @param engineName The name to assign.
     */
    void setName(const std::string& engineName) { name_ = engineName; }

    /**
     * @brief Sets the author of the engine.
     *
     * @param engineAuthor The name of the engine's author.
     */
    void setAuthor(const std::string& engineAuthor) { author_ = engineAuthor; }

    /**
     * @brief Sets the path to the engine executable.
     * @param path The executable path.
     */
    void setCmd(const std::string& path) { 
        cmd_ = path; 
    }

    /**
     * @brief Sets the working directory for the engine.
     * @param path The working directory path.
     */
    void setDir(const std::string& path) { dir_ = path; }

	/**
	 * @brief Sets the protocol used by the engine.
	 * @param proto The protocol to set (Uci, XBoard, etc.).
	 */
	void setProtocol(EngineProtocol proto) { protocol_ = proto; }
    /**
     * @brief Sets the protocol used by the engine from a string.
     * @param proto The protocol string.
     */
    void setProtocol(const std::string& proto);

    /**
     * @brief Gets the engine name.
     * @return The engine name.
     */
    [[nodiscard]] const std::string& getName() const { return name_; }

    /**
     * @brief Gets the engines author.
     * @return the engines author.
     */
    [[nodiscard]] const std::string& getAuthor() const { return author_; }

    /**
     * @brief Gets the path to the engine executable.
     * @return The executable path.
     */
    [[nodiscard]] const std::string& getCmd() const { return cmd_; }

    /**
     * @brief Gets the working directory.
     * @return The working directory path.
     */
    [[nodiscard]] const std::string& getDir() const { return dir_; }

	/**
	 * @brief Gets the protocol used by the engine.
	 * @return The engine protocol.
	 */
	[[nodiscard]] EngineProtocol getProtocol() const { return protocol_; }

	/**
	 * @brief Enables or disables pondering for the engine.
	 * @param enabled True to enable pondering, false to disable.
	 */
	void setPonder(bool enabled) { ponder_ = enabled; }
	/**
	 * @brief Checks if pondering is enabled.
	 * @return True if pondering is enabled, false otherwise.
	 */
	[[nodiscard]] bool isPonderEnabled() const { return ponder_; }

	/**
	 * @brief Enables or disables gauntlet mode.
	 * @param enabled True to enable gauntlet mode, false to disable.
	 */
	void setGauntlet(bool enabled) { gauntlet_ = enabled; }
	/**
	 * @brief Checks if gauntlet mode is enabled.
	 * @return True if gauntlet mode is enabled, false otherwise.
	 */
	[[nodiscard]] bool isGauntlet() const { return gauntlet_; }
    /**
     * @brief Provides mutable access to the gauntlet flag.
     * @return Reference to the gauntlet flag.
     */
    bool& gauntlet() { return gauntlet_; }

	/**
	 * @brief Sets whether scores are from white's point of view.
	 * @param enabled True if scores are from white's POV, false otherwise.
	 */
	void setScoreFromWhitePov(bool enabled) { scoreFromWhitePov_ = enabled; }
	/**
	 * @brief Checks if scores are from white's point of view.
	 * @return True if scores are from white's POV, false otherwise.
	 */
	[[nodiscard]] bool isScoreFromWhitePov() const { return scoreFromWhitePov_; }
    /**
     * @brief Provides mutable access to the score POV flag.
     * @return Reference to the score POV flag.
     */
    bool& scoreFromWhitePov() { return scoreFromWhitePov_; }

    /**
     * @brief Sets the time control for the engine.
     * @param tc The time control string.
     */
    void setTimeControl(const std::string& tc);
	/**
	 * @brief Gets the time control configuration.
	 * @return The time control object.
	 */
	[[nodiscard]] const TimeControl& getTimeControl() const { return tc_; }

    /**
     * @brief Sets the trace level for the engine.
     * @param level The trace level string.
     */
    void setTraceLevel(const std::string& level);
	/**
	 * @brief Gets the trace level.
	 * @return The trace level.
	 */
	[[nodiscard]] TraceLevel getTraceLevel() const { return traceLevel_; }
    /**
     * @brief Provides mutable access to the trace level.
     * @return Reference to the trace level.
     */
    TraceLevel& getTraceLevel() { return traceLevel_; }

    /**
     * @brief Returns the configured engine restart option.
     * @return The current RestartOption.
     */
    [[nodiscard]] RestartOption getRestartOption() const {
        return restart_;
    }

    /**
     * @brief Sets the engine restart option.
     * @param restart The RestartOption to use.
     */
    void setRestartOption(RestartOption restart) {
        restart_ = restart;
    }


    /**
     * @brief Gets the current option values.
     * @return A map of option names to their values.
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> getOptionValues() const {
        std::unordered_map<std::string, std::string> result;
        for (const auto& [_, opt] : optionValues_) {
            result[opt.originalName] = opt.value;
        }
        return result;
    }

    /**
     * @brief Sets a specific option value 
     * @param name The option name.
     * @param value The value to assign.
     */
    void setOptionValue(const std::string& name, const std::string& value) {
        std::string key = QaplaHelpers::to_lowercase(name);
        optionValues_[key] = OptionValue{ .originalName = name, .value = value };
    }

    /**
     * @brief Sets a specific value, overwriting any existing value.
     * @param name The option name.
     * @param value The value to assign.
	 */
    void setValue(const std::string& key, const std::string& value);

    /**
     * @brief Sets multiple values at once from a map of key-value pairs.
     * @param values A map of names and their values.
	 */
    void setValues(const std::unordered_map<std::string, std::string>& values) {
        for (const auto& [name, value] : values) {
            setValue(name, value);
        }
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
    [[nodiscard]] std::unordered_map<std::string, std::string> toDisambiguationMap() const;

    /**
	 * @brief Filters the current options based on the available options.
	 * @param availableOptions The set of options that are available for the engine.
	 * @return A map of option names and their values that are present in the available options.
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> getOptions(const EngineOptions& availableOptions) const;


    friend std::istream& operator>>(std::istream& in, EngineConfig& config);

    /**
     * @brief Saves the engine configuration to a stream (ini file format).
     * @param out The output stream to write the configuration to.
     * @param section The section name to use (default "engine").
     */
    void save(std::ostream& out, const std::string& section = "engine") const;
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

    struct OptionValue {
        std::string originalName;
        std::string value;
        bool operator==(const OptionValue&) const = default;
    };

    /**
	 * @brief Finalizes the set of options after parsing.
	 * @throws std::runtime_error If required options are missing or invalid.
     */
    void finalizeSetOptions();

    /**
     * @brief Checks whether engine name and command file name appear mismatched.
     * Emits a warning if normalized forms differ significantly.
	 * @param fileName The name of the engine executable.
	 * @param engineName The name of the engine as configured.
     */
    void warnOnNameMismatch(const std::string& fileName, const std::string& engineName) const;

    static std::string toString(const Value& value);
    std::string name_;
    std::string author_;
    std::string cmd_;
    std::string dir_;
    TimeControl tc_;
	TraceLevel traceLevel_ = TraceLevel::command;
    EngineProtocol protocol_ = EngineProtocol::Unknown;
    RestartOption restart_ = RestartOption::EngineDecides;
    bool ponder_ = false;
	bool gauntlet_ = false;
	bool scoreFromWhitePov_ = false;
    std::unordered_map<std::string, std::string> internalKeys_;
    std::unordered_map<std::string, OptionValue> optionValues_;
};

} // namespace QaplaTester
