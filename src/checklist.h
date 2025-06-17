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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Implements a list holding moves of a chess position
 * Moves are stored in one list - but different for "silent moves" and "non silent moves". Silent moves are moves 
 * not capturing and not promoting - non silent moves are captures and promotes.
 * Silent moves are pushed to the end and non silent moves are inserted to the front. As a result non silent moves
 * are always ordered first
 */

#pragma once

#include <string>
#include <unordered_map>
#include "logger.h"
#include "app-error.h"

class Checklist {
public:

    /**
     * @brief Classification of a check topic based on its relevance.
     */
    enum class CheckSection { Important, Missbehaviour, Notes, Report };

    /**
     * @brief Metadata describing a single logical check topic.
     */
    struct CheckTopic {
        std::string group;     
        std::string id;        
        std::string text;      
        CheckSection section;
    };

    /**
     * @brief Registers a check topic in the global topic registry.
     *        Must be called exactly once per topic ID.
     * @param topic The topic definition to register.
     * @throws std::runtime_error if a conflicting topic with the same ID already exists.
     */
    static void addTopic(const CheckTopic& topic);

    /**
     * @brief Returns the checklist instance associated with the given engine name.
     *        Creates a new instance if none exists yet.
     * @param engineName The name of the engine to retrieve the checklist for.
     * @return Pointer to the corresponding Checklist instance.
     */
    static inline Checklist* getChecklist(const std::string& engineName) {
        auto& ptr = checklists_[engineName];
        if (!ptr) {
            ptr = std::make_unique<Checklist>();
            ptr->engineName_ = engineName;
        }
        return ptr.get();
    }

    /**
     * @brief Reports the result of a check (success or failure).
     * @param topicId The unique identifier of the topic.
     * @param passed True if the check passed, false if it failed.
     */
    void report(const std::string& topicId, bool passed) {
        auto& stat = entries_[topicId];
        ++stat.total;
        if (!passed) ++stat.failures;
    }

    /**
     * @brief Reports a test result and logs details on failure (with early suppression).
     * @param topicId The topic ID.
     * @param passed True if the test passed; false if it failed.
     * @param detail Additional log message (only used on failure).
     * @param traceLevel Logging level (default is error).
     * @return True if passed; false otherwise.
     */
    bool logReport(const std::string& topicId, bool passed, std::string_view detail = "",
        TraceLevel traceLevel = TraceLevel::error);

	/**
	 * @brief sets the name of the author
	 * @param author The name of the author to set.
	 */
	void setEngineAuthor(const std::string& author) {
		engineAuthor_ = author;
	}

    /**
     * @brief Logs a summary of all results in this checklist.
     * @param traceLevel The minimum log level to output.
     * @return AppReturnCode indicating the most severe issue found.
     */
    AppReturnCode log(TraceLevel traceLevel);

    /**
     * @brief Logs the results of all engine checklists.
     *        Each engine is logged separately in registration order.
     * @param traceLevel The minimum log level to output.
     * @return The most severe AppReturnCode encountered across all engines.
     */
    static AppReturnCode logAll(TraceLevel traceLevel);

    /**
     * Reports the result of a single evaluation point for a topic.
     *
     * @param topic Logical topic name (e.g., "Move legality")
     * @param passed True if the check passed; false if it failed.
     */
    static void reportOld(const std::string topic, bool passed) {
        std::lock_guard lock(statsMutex_);
        auto& stat = stats_[topic];
        ++stat.total;
        if (!passed) ++stat.failures;
    }

    /**
	 * @brief Returns the number of errors for a given topic.
	 * @param topic The topic to check.
     */
    static int getNumErrors(const std::string& topic) {
        std::lock_guard lock(statsMutex_);
		auto& stat = stats_[topic];
		return stat.failures;
    }

	/**
	 * @brief Logs the result of a test and its details.
	 * @param name The name of the test.
	 * @param success True if the test passed; false if it failed.
	 * @param detail Additional details about the test that is logged on fail.
	 * @param traceLevel The trace level for the log entry (default is error).
     */ 
    static bool logCheck(const std::string name, bool success, std::string_view detail = "", 
        TraceLevel traceLevel = TraceLevel::error) {
        reportOld(name, success);
        if (!success) {
            auto numErrors = getNumErrors(name);
            if (numErrors > MAX_CLI_LOGS_PER_ERROR && traceLevel < TraceLevel::error) {
				return false; 
			}
            Logger::testLogger().log("[Report for topic \"" + std::string(name) + "\"] " + std::string(detail),
                numErrors > MAX_CLI_LOGS_PER_ERROR ? TraceLevel::info : traceLevel);
            if (numErrors == MAX_CLI_LOGS_PER_ERROR) {
                Logger::testLogger().log("Too many similar reports. Further reports of this type will be suppressed. ", traceLevel);
            }
        }
        return success;
    }

    /**
	 * @brief Logs the results of all tests to the test logger.
     */
    static AppReturnCode logOld(TraceLevel traceLevel = TraceLevel::result);

    /**
	 * @brief Sets the engine name and author.
     */ 
	static void setEngine(const std::string& name, const std::string& author) {
		name_ = name;
		author_ = author;
	}

	/**
      * @brief Clears all stored statistics.
      */
	static void clear() {
		std::lock_guard lock(statsMutex_);
		stats_.clear();
        name_.clear();
		author_.clear();
	}

	static inline bool reportUnderruns = false;

private:


    static constexpr uint32_t MAX_CLI_LOGS_PER_ERROR = 5;
    static constexpr uint32_t MAX_FILE_LOGS_PER_ERROR = 10;
    struct Stat {
        int total = 0;
        int failures = 0;
    };

    static void addMissingTopicsAsFail();

    static inline std::string name_;
    static inline std::string author_;
    static inline std::mutex statsMutex_;
    static inline std::unordered_map<std::string, Stat> stats_;


    struct CheckEntry {
        int total = 0;
        int failures = 0;
    };

    static inline std::unordered_map<std::string, CheckTopic> knownTopics_;
    static inline std::unordered_map<std::string, std::unique_ptr<Checklist>> checklists_;
    std::string engineName_;
    std::string engineAuthor_;
    std::unordered_map<std::string, CheckEntry> entries_;
};
