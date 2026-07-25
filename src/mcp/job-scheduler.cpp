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

#include "job-scheduler.h"

#include "../base-elements/logger.h"
#include "../cli/app-runner.h"
#include "../cli/task-types.h"

#include <cctype>
#include <filesystem>
#include <format>

namespace QaplaTester::Mcp {

JobScheduler& JobScheduler::instance() {
    static JobScheduler instance;
    return instance;
}

JobScheduler::~JobScheduler() {
    stop();
}

void JobScheduler::configure(JobExecutor executeCallback, CancelExecutor cancelCallback) {
    std::scoped_lock lock(mutex_);
    executeCallback_ = std::move(executeCallback);
    cancelCallback_ = std::move(cancelCallback);
}

void JobScheduler::start() {
    std::scoped_lock lock(mutex_);
    if (workerStarted_) {
        return;
    }

    stopRequested_ = false;
    workerStarted_ = true;
    workerThread_ = std::thread([this]() {
        runWorker();
    });
}

void JobScheduler::stop() {
    {
        std::scoped_lock lock(mutex_);
        if (!workerStarted_) {
            return;
        }
        stopRequested_ = true;
        for (auto& queuedJob : queuedJobs_) {
            queuedJob.state = QueueJobState::Canceled;
            queuedJob.finishedAt = std::chrono::system_clock::now();
            queuedJob.errorMessage = "Canceled during scheduler shutdown.";
            pushHistoryLocked(queuedJob);
        }
        queuedJobs_.clear();
    }

    condition_.notify_all();

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    std::scoped_lock lock(mutex_);
    workerStarted_ = false;
}

std::string JobScheduler::enqueue(QueueJob jobEntry) {
    std::string createdJobId;
    {
        std::scoped_lock lock(mutex_);
        const auto nextNumber = nextJobNumber_.fetch_add(1U);
        createdJobId = std::format("job-{}", nextNumber);
        jobEntry.jobId = createdJobId;
        jobEntry.createdAt = std::chrono::system_clock::now();
        jobEntry.state = QueueJobState::Queued;
        queuedJobs_.push_back(std::move(jobEntry));
    }

    condition_.notify_all();
    return createdJobId;
}

bool JobScheduler::cancelJob(const std::string& jobId, bool niceStop) {
    bool cancelActive = false;
    {
        std::scoped_lock lock(mutex_);

        for (auto iter = queuedJobs_.begin(); iter != queuedJobs_.end(); ++iter) {
            if (iter->jobId == jobId) {
                iter->state = QueueJobState::Canceled;
                iter->finishedAt = std::chrono::system_clock::now();
                iter->errorMessage = "Canceled before execution.";
                pushHistoryLocked(*iter);
                queuedJobs_.erase(iter);
                return true;
            }
        }

        if (activeJob_.has_value() && activeJob_->jobId == jobId) {
            activeJob_->cancellationRequested = true;
            cancelActive = true;
        }
    }

    if (cancelActive && cancelCallback_) {
        cancelCallback_(niceStop);
    }

    return cancelActive;
}

bool JobScheduler::requestCancelActive(bool niceStop) {
    bool hasActiveJob = false;
    {
        std::scoped_lock lock(mutex_);
        if (!activeJob_.has_value()) {
            return false;
        }

        activeJob_->cancellationRequested = true;
        hasActiveJob = true;
    }

    if (hasActiveJob && cancelCallback_) {
        cancelCallback_(niceStop);
    }
    return hasActiveJob;
}

size_t JobScheduler::clearQueuedJobs() {
    std::scoped_lock lock(mutex_);

    const auto removedCount = queuedJobs_.size();
    const auto now = std::chrono::system_clock::now();
    for (auto& queuedJob : queuedJobs_) {
        queuedJob.state = QueueJobState::Canceled;
        queuedJob.finishedAt = now;
        queuedJob.errorMessage = "Canceled by clear queue.";
        pushHistoryLocked(queuedJob);
    }
    queuedJobs_.clear();
    return removedCount;
}

Json::JsonValue JobScheduler::queueStatusJson() const {
    std::scoped_lock lock(mutex_);

    auto root = Json::JsonValue::object();
    root["running"] = activeJob_.has_value();
    root["queue_length"] = static_cast<double>(queuedJobs_.size());

    if (activeJob_.has_value()) {
        auto& active = root["active_job"];
        active["job_id"] = activeJob_->jobId;
        active["type"] = typeName(activeJob_->jobType);
        active["job_intent"] = activeJob_->jobIntent;
        active["state"] = stateName(activeJob_->state);
        active["created_at"] = toUnixSeconds(activeJob_->createdAt);
        active["started_at"] = toUnixSeconds(activeJob_->startedAt);
        active["cancel_requested"] = activeJob_->cancellationRequested;
    }

    auto& queuedArray = root["queued_jobs"] = Json::JsonValue::array();
    for (const auto& queuedJob : queuedJobs_) {
        auto& entry = queuedArray[queuedArray.size()];
        entry["job_id"] = queuedJob.jobId;
        entry["type"] = typeName(queuedJob.jobType);
        entry["job_intent"] = queuedJob.jobIntent;
        entry["state"] = stateName(queuedJob.state);
        entry["created_at"] = toUnixSeconds(queuedJob.createdAt);
    }

    auto& finishedArray = root["finished_jobs"] = Json::JsonValue::array();
    for (const auto& finishedJob : finishedJobs_) {
        auto& entry = finishedArray[finishedArray.size()];
        entry["job_id"] = finishedJob.jobId;
        entry["type"] = typeName(finishedJob.jobType);
        entry["job_intent"] = finishedJob.jobIntent;
        entry["state"] = stateName(finishedJob.state);
        entry["created_at"] = toUnixSeconds(finishedJob.createdAt);
        entry["started_at"] = toUnixSeconds(finishedJob.startedAt);
        entry["finished_at"] = toUnixSeconds(finishedJob.finishedAt);
        entry["return_code"] = static_cast<double>(static_cast<int>(finishedJob.returnCode));
        entry["return_code_text"] = finishedJob.returnCodeText;
        if (!finishedJob.reportUri.empty()) {
            entry["report_uri"] = finishedJob.reportUri;
        }
        if (!finishedJob.resultUri.empty()) {
            entry["result_uri"] = finishedJob.resultUri;
        }
        if (!finishedJob.taskStatus.is_null()) {
            entry["task_status"] = finishedJob.taskStatus;
        }
        if (!finishedJob.errorMessage.empty()) {
            entry["error"] = finishedJob.errorMessage;
        }
    }

    return root;
}

Json::JsonValue JobScheduler::finishedResultsJson() const {
    const auto statusSnapshot = queueStatusJson();

    auto root = Json::JsonValue::object();
    const auto& results = statusSnapshot.at("finished_jobs");
    root["count"] = static_cast<double>(results.size());
    root["results"] = results;
    return root;
}

size_t JobScheduler::clearFinishedResults() {
    std::scoped_lock lock(mutex_);
    const auto removedCount = finishedJobs_.size();
    finishedJobs_.clear();
    return removedCount;
}

void JobScheduler::runWorker() {
    while (true) {
        QueueJob jobToRun;
        if (!waitAndActivateNextJob(jobToRun)) {
            return;
        }

        AppReturnCode returnCode = AppReturnCode::GeneralError;
        std::string errorMessage;

        try {
            if (!executeCallback_) {
                throw AppError::make(1, AppReturnCode::GeneralError, "No scheduler executor configured.");
            }
            returnCode = executeCallback_(jobToRun);
        } catch (const std::exception& exception) {
            errorMessage = exception.what();
            returnCode = AppReturnCode::GeneralError;
        } catch (...) {
            errorMessage = "Unknown exception during queued job execution.";
            returnCode = AppReturnCode::GeneralError;
        }

        finalizeActiveJob(returnCode, errorMessage);
    }
}

bool JobScheduler::waitAndActivateNextJob(QueueJob& jobToRun) {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this]() {
        return stopRequested_ || !queuedJobs_.empty();
    });

    if (stopRequested_ && queuedJobs_.empty()) {
        return false;
    }

    jobToRun = std::move(queuedJobs_.front());
    queuedJobs_.pop_front();
    jobToRun.state = QueueJobState::Running;
    jobToRun.startedAt = std::chrono::system_clock::now();
    activeJob_ = jobToRun;
    return true;
}

void JobScheduler::finalizeActiveJob(AppReturnCode code, const std::string& errorMessage) {
    std::scoped_lock lock(mutex_);
    if (!activeJob_.has_value()) {
        return;
    }

    activeJob_->finishedAt = std::chrono::system_clock::now();
    activeJob_->returnCode = code;
    activeJob_->returnCodeText = returnCodeName(code);
    if (!errorMessage.empty()) {
        activeJob_->errorMessage = errorMessage;
    }

    enrichFinishedJob(*activeJob_);

    if (activeJob_->cancellationRequested) {
        activeJob_->state = QueueJobState::Canceled;
        if (activeJob_->errorMessage.empty()) {
            activeJob_->errorMessage = "Canceled while running.";
        }
    } else {
        activeJob_->state = isFailedReturnCode(code) ? QueueJobState::Failed : QueueJobState::Succeeded;
    }

    pushHistoryLocked(*activeJob_);
    activeJob_.reset();
}

void JobScheduler::enrichFinishedJob(QueueJob& jobEntry) {
    const auto reportFilename = std::filesystem::path(Logger::reportLogger().getFilename()).filename().string();
    jobEntry.reportFilename = reportFilename;
    if (!reportFilename.empty()) {
        jobEntry.reportUri = std::format("qapla://reports/{}/{}", jobEntry.toolName, reportFilename);
    }

    const auto copyResultFileFromSection = [&jobEntry](const std::string& sectionName) {
        if (const auto sectionMap = jobEntry.configData.getSectionMap(sectionName)) {
            for (const auto& [_, sections] : *sectionMap) {
                for (const auto& section : sections) {
                    for (const auto& [key, value] : section.entries) {
                        if (key == "file") {
                            jobEntry.resultUri = std::format(
                                "qapla://reports/{}/{}",
                                sectionName,
                                std::filesystem::path(value).filename().string());
                        }
                    }
                }
            }
        }
    };

    if (jobEntry.jobType == QueueJobType::Sprt) {
        copyResultFileFromSection("sprt");
    }

    if (jobEntry.jobType == QueueJobType::Tournament) {
        copyResultFileFromSection("tournament");
    }

    const auto taskType = Cli::getTaskType(jobEntry.toolName);
    if (taskType != Cli::TaskType::None) {
        jobEntry.taskStatus = AppRunner::getTaskStatus(taskType);
    }
}

std::string JobScheduler::stateName(QueueJobState state) {
    switch (state) {
        case QueueJobState::Queued:
            return "queued";
        case QueueJobState::Running:
            return "running";
        case QueueJobState::Succeeded:
            return "succeeded";
        case QueueJobState::Failed:
            return "failed";
        case QueueJobState::Canceled:
            return "canceled";
        default:
            return "unknown";
    }
}

std::string JobScheduler::typeName(QueueJobType type) {
    switch (type) {
        case QueueJobType::Sprt:
            return "sprt";
        case QueueJobType::Tournament:
            return "tournament";
        case QueueJobType::Epd:
            return "epd";
        case QueueJobType::Spsa:
            return "spsa";
        case QueueJobType::Clop:
            return "clop";
        case QueueJobType::Test:
            return "test";
        default:
            return "unknown";
    }
}

std::string JobScheduler::returnCodeName(AppReturnCode code) {
    auto codeName = appReturnCodeName(code);
    bool numericOnly = !codeName.empty();
    for (const auto currentCharacter : codeName) {
        if (std::isdigit(static_cast<unsigned char>(currentCharacter)) == 0) {
            numericOnly = false;
            break;
        }
    }

    if (numericOnly) {
        return std::format("ReturnCode({})", codeName);
    }
    return codeName;
}

double JobScheduler::toUnixSeconds(const std::chrono::system_clock::time_point& timePoint) {
    if (timePoint == std::chrono::system_clock::time_point{}) {
        return 0.0;
    }

    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    return static_cast<double>(milliseconds) / 1000.0;
}

bool JobScheduler::isFailedReturnCode(AppReturnCode code) {
    return code == AppReturnCode::GeneralError ||
        code == AppReturnCode::InvalidParameters ||
        code == AppReturnCode::EngineError ||
        code == AppReturnCode::EngineMissbehaviour;
}

void JobScheduler::pushHistoryLocked(const QueueJob& jobEntry) {
    finishedJobs_.push_back(jobEntry);
}

} // namespace QaplaTester::Mcp
