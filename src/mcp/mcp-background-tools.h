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

#include "../base-elements/qapla-json.h"
#include "job-scheduler.h"

#include <string>
#include <string_view>

namespace QaplaTester::Mcp {

/**
 * @brief Evaluates whether a tool-call argument object requests background execution.
 * @param arguments The MCP tool arguments object.
 * @return True when background execution is requested.
 */
[[nodiscard]] bool isBackgroundRequested(const Json::JsonValue::Object& arguments);

/**
 * @brief Creates control status JSON with runner and queue information.
 * @return Combined runner and queue status.
 */
[[nodiscard]] Json::JsonValue createCombinedControlStatus();

/**
 * @brief Checks whether a tool supports queued background execution.
 */
[[nodiscard]] bool isQueueableTool(std::string_view toolName);

/**
 * @brief Maps MCP tool name to queue job type.
 */
[[nodiscard]] QueueJobType queueJobTypeForTool(std::string_view toolName);

/**
 * @brief Extracts and validates job intent from tool arguments for queueable tools.
 * @param toolName The tool name.
 * @param toolArgs Mutable argument object; removes job_intent on success.
 * @param background Whether background mode is requested.
 * @return The validated job intent string, or empty when not required.
 */
[[nodiscard]] std::string extractJobIntentForQueue(
	std::string_view toolName,
	Json::JsonValue::Object& toolArgs,
	bool background);

/**
 * @brief Creates queue start summary text for a newly enqueued job.
 */
[[nodiscard]] std::string createQueueStartSummary(
	std::string_view toolName,
	std::string_view jobId,
	const Json::JsonValue& queueStatus);

} // namespace QaplaTester::Mcp
