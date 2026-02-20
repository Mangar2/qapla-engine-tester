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

JsonValue JobScheduler::queueStatusJson() const {
    std::scoped_lock lock(mutex_);

    JsonValue::Object root;
    root["running"] = JsonHelper::makeBool(activeJob_.has_value());
    root["queue_length"] = JsonHelper::makeNumber(static_cast<double>(queuedJobs_.size()));

    if (activeJob_.has_value()) {
        JsonValue::Object activeObject;
        activeObject["job_id"] = JsonHelper::makeString(activeJob_->jobId);
        activeObject["type"] = JsonHelper::makeString(typeName(activeJob_->jobType));
        activeObject["job_intent"] = JsonHelper::makeString(activeJob_->jobIntent);
        activeObject["state"] = JsonHelper::makeString(stateName(activeJob_->state));
        activeObject["created_at"] = JsonHelper::makeNumber(toUnixSeconds(activeJob_->createdAt));
        activeObject["started_at"] = JsonHelper::makeNumber(toUnixSeconds(activeJob_->startedAt));
        activeObject["cancel_requested"] = JsonHelper::makeBool(activeJob_->cancellationRequested);
        root["active_job"] = JsonHelper::makeObject(std::move(activeObject));
    }

    JsonValue::Array queuedArray;
    for (const auto& queuedJob : queuedJobs_) {
        JsonValue::Object queuedObject;
        queuedObject["job_id"] = JsonHelper::makeString(queuedJob.jobId);
        queuedObject["type"] = JsonHelper::makeString(typeName(queuedJob.jobType));
        queuedObject["job_intent"] = JsonHelper::makeString(queuedJob.jobIntent);
        queuedObject["state"] = JsonHelper::makeString(stateName(queuedJob.state));
        queuedObject["created_at"] = JsonHelper::makeNumber(toUnixSeconds(queuedJob.createdAt));
        queuedArray.push_back(JsonHelper::makeObject(std::move(queuedObject)));
    }
    root["queued_jobs"] = JsonValue{ .data = std::move(queuedArray) };

    JsonValue::Array finishedArray;
    for (const auto& finishedJob : finishedJobs_) {
        JsonValue::Object finishedObject;
        finishedObject["job_id"] = JsonHelper::makeString(finishedJob.jobId);
        finishedObject["type"] = JsonHelper::makeString(typeName(finishedJob.jobType));
        finishedObject["job_intent"] = JsonHelper::makeString(finishedJob.jobIntent);
        finishedObject["state"] = JsonHelper::makeString(stateName(finishedJob.state));
        finishedObject["created_at"] = JsonHelper::makeNumber(toUnixSeconds(finishedJob.createdAt));
        finishedObject["started_at"] = JsonHelper::makeNumber(toUnixSeconds(finishedJob.startedAt));
        finishedObject["finished_at"] = JsonHelper::makeNumber(toUnixSeconds(finishedJob.finishedAt));
        finishedObject["return_code"] = JsonHelper::makeNumber(static_cast<double>(static_cast<int>(finishedJob.returnCode)));
        finishedObject["return_code_text"] = JsonHelper::makeString(finishedJob.returnCodeText);
        if (!finishedJob.reportUri.empty()) {
            finishedObject["report_uri"] = JsonHelper::makeString(finishedJob.reportUri);
        }
        if (!finishedJob.resultUri.empty()) {
            finishedObject["result_uri"] = JsonHelper::makeString(finishedJob.resultUri);
        }
        if (!finishedJob.errorMessage.empty()) {
            finishedObject["error"] = JsonHelper::makeString(finishedJob.errorMessage);
        }
        finishedArray.push_back(JsonHelper::makeObject(std::move(finishedObject)));
    }
    root["finished_jobs"] = JsonValue{ .data = std::move(finishedArray) };

    return JsonHelper::makeObject(std::move(root));
}

JsonValue JobScheduler::finishedResultsJson() const {
    std::scoped_lock lock(mutex_);

    JsonValue::Object root;
    root["count"] = JsonHelper::makeNumber(static_cast<double>(finishedJobs_.size()));

    JsonValue::Array results;
    for (const auto& finishedJob : finishedJobs_) {
        JsonValue::Object result;
        result["job_id"] = JsonHelper::makeString(finishedJob.jobId);
        result["type"] = JsonHelper::makeString(typeName(finishedJob.jobType));
        result["job_intent"] = JsonHelper::makeString(finishedJob.jobIntent);
        result["state"] = JsonHelper::makeString(stateName(finishedJob.state));
        result["return_code"] = JsonHelper::makeNumber(static_cast<double>(static_cast<int>(finishedJob.returnCode)));
        result["return_code_text"] = JsonHelper::makeString(finishedJob.returnCodeText);
        result["created_at"] = JsonHelper::makeNumber(toUnixSeconds(finishedJob.createdAt));
        result["started_at"] = JsonHelper::makeNumber(toUnixSeconds(finishedJob.startedAt));
        result["finished_at"] = JsonHelper::makeNumber(toUnixSeconds(finishedJob.finishedAt));
        if (!finishedJob.reportUri.empty()) {
            result["report_uri"] = JsonHelper::makeString(finishedJob.reportUri);
        }
        if (!finishedJob.resultUri.empty()) {
            result["result_uri"] = JsonHelper::makeString(finishedJob.resultUri);
        }
        if (!finishedJob.errorMessage.empty()) {
            result["error"] = JsonHelper::makeString(finishedJob.errorMessage);
        }
        results.push_back(JsonHelper::makeObject(std::move(result)));
    }

    root["results"] = JsonValue{ .data = std::move(results) };
    return JsonHelper::makeObject(std::move(root));
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
        case QueueJobType::Test:
            return "test";
        default:
            return "unknown";
    }
}

std::string JobScheduler::returnCodeName(AppReturnCode code) {
    switch (code) {
        case AppReturnCode::NoError:
            return "NoError";
        case AppReturnCode::GeneralError:
            return "GeneralError";
        case AppReturnCode::InvalidParameters:
            return "InvalidParameters";
        case AppReturnCode::EngineError:
            return "EngineError";
        case AppReturnCode::EngineMissbehaviour:
            return "EngineMissbehaviour";
        case AppReturnCode::EngineNote:
            return "EngineNote";
        case AppReturnCode::MissedTarget:
            return "MissedTarget";
        case AppReturnCode::H1Accepted:
            return "H1Accepted";
        case AppReturnCode::H0Accepted:
            return "H0Accepted";
        case AppReturnCode::UndefinedResult:
            return "UndefinedResult";
        default:
            return std::format("ReturnCode({})", static_cast<int>(code));
    }
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
