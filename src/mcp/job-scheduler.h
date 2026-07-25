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

#include "../base-elements/app-error.h"
#include "../base-elements/ini-file.h"
#include "../base-elements/qapla-json.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace QaplaTester::Mcp {

/**
 * @brief Supported queue job categories.
 */
enum class QueueJobType {
    Sprt,
    Tournament,
    Epd,
    Spsa,
    Clop,
    Test
};

/**
 * @brief Queue lifecycle states for a submitted job.
 */
enum class QueueJobState {
    Queued,
    Running,
    Succeeded,
    Failed,
    Canceled
};

/**
 * @brief Immutable payload and mutable runtime state for one queued job.
 */
struct QueueJob {
    std::string jobId;
    QueueJobType jobType = QueueJobType::Sprt;
    std::string toolName;
    std::string jobIntent;
    std::string reportBaseName;
    QaplaHelpers::ConfigData configData;
    Json::JsonValue::Object executionArguments;

    QueueJobState state = QueueJobState::Queued;
    AppReturnCode returnCode = AppReturnCode::NoError;
    std::string returnCodeText;
    std::string errorMessage;
    std::string reportFilename;
    std::string reportUri;
    std::string resultUri;
    Json::JsonValue taskStatus;

    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point finishedAt;
    bool cancellationRequested = false;
};

/**
 * @brief Single-worker scheduler for sequential MCP background jobs.
 *
 * @details Designed as generic queue infrastructure. Currently only SPRT jobs
 * are enqueued by MCP tool routing, but type handling is extensible.
 */
class JobScheduler {
public:
    using JobExecutor = std::function<AppReturnCode(const QueueJob&)>;
    using CancelExecutor = std::function<void(bool)>;

    /**
     * @brief Returns global scheduler singleton.
     */
    static JobScheduler& instance();

    JobScheduler(const JobScheduler&) = delete;
    JobScheduler& operator=(const JobScheduler&) = delete;

    /**
     * @brief Sets execution callbacks for queued jobs and cancellation.
     */
    void configure(JobExecutor executeCallback, CancelExecutor cancelCallback);

    /**
     * @brief Starts scheduler worker thread.
     */
    void start();

    /**
     * @brief Stops scheduler worker thread and clears queued jobs.
     */
    void stop();

    /**
     * @brief Enqueues a new job and returns generated job id.
     */
    [[nodiscard]] std::string enqueue(QueueJob jobEntry);

    /**
     * @brief Cancels a queued or running job by id.
     * @return True if a job with the id existed and cancel was requested/applied.
     */
    [[nodiscard]] bool cancelJob(const std::string& jobId, bool niceStop);

    /**
     * @brief Requests cancel for active job if any.
     * @return True if a running job existed.
     */
    [[nodiscard]] bool requestCancelActive(bool niceStop);

    /**
     * @brief Removes all jobs that are still queued.
     * @return Number of removed jobs.
     */
    [[nodiscard]] size_t clearQueuedJobs();

    /**
     * @brief Creates queue status JSON for MCP responses.
     */
    [[nodiscard]] Json::JsonValue queueStatusJson() const;

    /**
     * @brief Creates detailed JSON result list for all finished queue jobs.
     */
    [[nodiscard]] Json::JsonValue finishedResultsJson() const;

    /**
     * @brief Clears all finished queue job results.
     * @return Number of removed finished entries.
     */
    [[nodiscard]] size_t clearFinishedResults();

private:
    JobScheduler() = default;
    ~JobScheduler();

    void runWorker();
    bool waitAndActivateNextJob(QueueJob& jobToRun);
    void finalizeActiveJob(AppReturnCode code, const std::string& errorMessage);
    static void enrichFinishedJob(QueueJob& jobEntry);
    static std::string stateName(QueueJobState state);
    static std::string typeName(QueueJobType type);
    static std::string returnCodeName(AppReturnCode code);
    static double toUnixSeconds(const std::chrono::system_clock::time_point& timePoint);
    static bool isFailedReturnCode(AppReturnCode code);
    void pushHistoryLocked(const QueueJob& jobEntry);

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<QueueJob> queuedJobs_;
    std::optional<QueueJob> activeJob_;
    std::vector<QueueJob> finishedJobs_;

    JobExecutor executeCallback_;
    CancelExecutor cancelCallback_;

    std::thread workerThread_;
    bool stopRequested_ = false;
    bool workerStarted_ = false;
    std::atomic<uint64_t> nextJobNumber_ = 1;
};

} // namespace QaplaTester::Mcp
