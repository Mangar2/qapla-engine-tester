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

class Checklist {
public:
    /**
     * Reports the result of a single evaluation point for a topic.
     *
     * @param topic Logical topic name (e.g., "Move legality")
     * @param passed True if the check passed; false if it failed.
     */
    static void report(const std::string topic, bool passed) {
        auto& stat = stats_[topic];
        ++stat.total;
        if (!passed) ++stat.failures;
    }

    /**
	 * @brief Returns the number of errors for a given topic.
	 * @param topic The topic to check.
     */
    static int getNumErrors(const std::string& topic) {
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
        report(name, success);
        if (!success) {
            auto numErrors = getNumErrors(name);
            Logger::testLogger().log("[Report for topic \"" + std::string(name) + "\"] " + std::string(detail),
                numErrors > MAX_CLI_LOGS_PER_ERROR ? TraceLevel::info : traceLevel);
            if (numErrors == MAX_CLI_LOGS_PER_ERROR) {
                Logger::testLogger().log("Further reports of this type will be suppressed. See log for full details.");
            }
        }
        return success;
    }

    /**
	 * @brief Logs the results of all tests to the test logger.
     */
    static void log();

    /**
	 * @brief Sets the engine name and author.
     */ 
	static void setEngine(const std::string& name, const std::string& author) {
		name_ = name;
		author_ = author;
	}


private:
    static constexpr uint32_t MAX_CLI_LOGS_PER_ERROR = 5;
    struct Stat {
        int total = 0;
        int failures = 0;
    };

    static void addMissingTopicsAsFail();

    static inline std::string name_;
    static inline std::string author_;

    static inline std::unordered_map<std::string, Stat> stats_;
};
