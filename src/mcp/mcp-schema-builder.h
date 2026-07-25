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
#include "../cli/settings-manager.h"
#include <string>
#include <string_view>
#include <vector>

namespace QaplaTester::Mcp {

/**
 * @brief Helper class to build MCP tool schemas.
 * Extracts logic from McpServer to provide a cleaner implementation.
 */
class McpSchemaBuilder {
public:
    struct ToolInfo {
        std::string_view name;
        std::string_view description;
        std::vector<std::string_view> groups;
    };

    /**
     * @brief Creates the input schema for a specific tool.
     * @param info The tool information structure.
     * @param registeredNames A string containing a comma-separated list of registered engine names.
     * @return The JSON object representing the input schema.
     */
    [[nodiscard]] static Json::JsonValue createInputSchema(const ToolInfo& info, const std::string& registeredNames);

private:
    static void addParametersFromGroup(std::string_view groupName, Json::JsonValue& properties);
    static void addArrayGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, Json::JsonValue& properties);
    static void addSingleGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, Json::JsonValue& properties);
    static void addGlobalParameterSchema(const std::string& key, Json::JsonValue& properties);

    // Specific tool helpers
    static void addReadReportSchema(Json::JsonValue& properties, Json::JsonValue& required);
    static void addControlSchema(Json::JsonValue& properties, Json::JsonValue& required);
    static void addManageEnginesSchema(Json::JsonValue& properties, Json::JsonValue& required, const std::string& registeredNames);
    static void addNonTaskSchema(const ToolInfo& info, Json::JsonValue& properties);
    static void addStandardTaskSchema(const ToolInfo& info, Json::JsonValue& properties, Json::JsonValue& required, const std::string& registeredNames);
};

} // namespace QaplaTester::Mcp
