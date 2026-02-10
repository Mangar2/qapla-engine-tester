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

#include "json-helper.h"
#include "../engine-handling/engine-capabilities.h"
#include "../cli/task-types.h"
#include <string>

namespace QaplaTester::Mcp {

class McpEngineTool {
public:
    static JsonValue::Array handleManageEngines(const JsonValue::Object& arguments, QaplaConfiguration::EngineCapabilities& capabilities);
    static void setupActiveEngines(const JsonValue::Object& arguments, Cli::TaskType taskType, QaplaConfiguration::EngineCapabilities& capabilities);

private:
    [[nodiscard]] static std::string listEngines();
    [[nodiscard]] static std::string getEngineDetails(const JsonValue::Object& arguments, const QaplaConfiguration::EngineCapabilities& capabilities);
    static std::string addOrUpdateEngine(const JsonValue::Object& arguments, bool isUpdate, QaplaConfiguration::EngineCapabilities& capabilities);
    static std::string copyEngine(const JsonValue::Object& arguments);
    static std::string updateAllEngines(const JsonValue::Object& arguments, QaplaConfiguration::EngineCapabilities& capabilities);

    static void applyGlobalTimeControl(const std::vector<std::string>& engineNames, const JsonValue& tcValue, QaplaConfiguration::EngineCapabilities& capabilities);
    static void syncToEngineRegistry(const std::string& engineName);
};

} // namespace QaplaTester::Mcp
