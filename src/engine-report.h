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
#include "tournament-result.h"

class EngineReport {
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
    static inline EngineReport* getChecklist(const std::string& engineName) {
        auto& ptr = checklists_[engineName];
        if (!ptr) {
            ptr = std::make_unique<EngineReport>();
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
        std::lock_guard<std::mutex> lock(statsMutex_);
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
     * @brief Logs a summary of all results in this checklist.
     * @param traceLevel The minimum log level to output.
     * @return AppReturnCode indicating the most severe issue found.
     */
    AppReturnCode log(TraceLevel traceLevel, const std::optional<EngineResult>& result);

    /**
     * @brief Logs the results of all engine checklists.
     *        Each engine is logged separately in registration order.
     * @param traceLevel The minimum log level to output.
	 * @param result The TournamentResult containing all engine results.
     * @return The most severe AppReturnCode encountered across all engines.
     */
    static AppReturnCode logAll(TraceLevel traceLevel, const std::optional<TournamentResult>& result = std::nullopt);

	void setAuthor(const std::string& author) {
        std::lock_guard<std::mutex> lock(statsMutex_);
		engineAuthor_ = author;
	}

	static inline bool reportUnderruns = false;

    void setTournamentResult(const EngineResult& result) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        engineResult_ = result;
    }
private:

    static constexpr uint32_t MAX_CLI_LOGS_PER_ERROR = 2;
    static constexpr uint32_t MAX_FILE_LOGS_PER_ERROR = 10;
    static inline std::mutex statsMutex_;

    struct CheckEntry {
        uint32_t total = 0;
        uint32_t failures = 0;
    };

    static inline std::vector<CheckTopic> registeredTopics_;
    static inline std::unordered_map<std::string, std::unique_ptr<EngineReport>> checklists_;
    std::string engineName_;
    std::string engineAuthor_;
    std::unordered_map<std::string, CheckEntry> entries_;

    EngineResult engineResult_;
};
