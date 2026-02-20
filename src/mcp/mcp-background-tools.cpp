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

#include "mcp-background-tools.h"

#include "job-scheduler.h"
#include "../base-elements/app-error.h"
#include "../cli/app-runner.h"

#include <algorithm>
#include <array>
#include <format>

namespace QaplaTester::Mcp {

bool isBackgroundRequested(const JsonValue::Object& arguments) {
    if (const auto iterator = arguments.find("mcp_background"); iterator != arguments.end() && iterator->second.isBool()) {
        return iterator->second.asBool();
    }

    if (const auto iterator = arguments.find("background"); iterator != arguments.end() && iterator->second.isBool()) {
        return iterator->second.asBool();
    }

    return false;
}

std::string createCombinedControlStatus() {
    auto runnerStatus = AppRunner::getStatus();
    std::string_view runnerStatusView = runnerStatus;
    auto runnerJson = JsonHelper::parse(runnerStatusView);
    if (!runnerJson.isObject()) {
        return runnerStatus;
    }

    auto rootObject = runnerJson.asObject();
    rootObject["job_queue"] = JobScheduler::instance().queueStatusJson();
    return JsonHelper::serialize(JsonHelper::makeObject(std::move(rootObject)));
}

bool isQueueableTool(std::string_view toolName) {
    static constexpr std::array queueableTools{
        std::string_view{"sprt"},
        std::string_view{"tournament"},
        std::string_view{"epd"},
        std::string_view{"spsa"},
        std::string_view{"test"}
    };

    return std::ranges::any_of(queueableTools, [toolName](std::string_view knownName) {
        return knownName == toolName;
    });
}

QueueJobType queueJobTypeForTool(std::string_view toolName) {
    if (toolName == "sprt") {
        return QueueJobType::Sprt;
    }

    if (toolName == "tournament") {
        return QueueJobType::Tournament;
    }

    if (toolName == "epd") {
        return QueueJobType::Epd;
    }

    if (toolName == "spsa") {
        return QueueJobType::Spsa;
    }

    if (toolName == "test") {
        return QueueJobType::Test;
    }

    throw AppError::makeInvalidParameters(std::format("Tool '{}' is not queueable.", toolName));
}

std::string extractJobIntentForQueue(
    std::string_view toolName,
    JsonValue::Object& toolArgs,
    bool background)
{
    if (!isQueueableTool(toolName)) {
        return "";
    }

    if (!toolArgs.contains("job_intent")) {
        if (background) {
            throw AppError::makeInvalidParameters(
                std::format("String job_intent is required for {} jobs.", toolName));
        }
        return "";
    }

    if (!toolArgs.at("job_intent").isString()) {
        throw AppError::makeInvalidParameters(
            std::format("String job_intent is required for {} jobs.", toolName));
    }

    auto jobIntent = toolArgs.at("job_intent").asString();
    toolArgs.erase("job_intent");

    if (jobIntent.empty()) {
        throw AppError::makeInvalidParameters(
            std::format("String job_intent must not be empty for {} jobs.", toolName));
    }

    return jobIntent;
}

std::string createQueueStartSummary(
    std::string_view toolName,
    std::string_view jobId,
    std::string_view queueStatusJson)
{
    auto summary = std::format("Tool '{}' queued as '{}'.", toolName, jobId);
    summary += std::format("\nThe next queued '{}' job starts automatically after the running one finishes.", toolName);
    summary += "\nUse control/status to monitor and control/cancel_job to stop specific jobs.";
    summary += std::format("\nQueue status: {}", queueStatusJson);
    return summary;
}

} // namespace QaplaTester::Mcp
