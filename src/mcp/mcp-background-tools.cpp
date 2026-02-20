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
#include "../cli/app-runner.h"

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

} // namespace QaplaTester::Mcp
