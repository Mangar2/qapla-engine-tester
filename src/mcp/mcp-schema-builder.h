#pragma once

#include "json-helper.h"
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
    [[nodiscard]] static JsonValue::Object createInputSchema(const ToolInfo& info, const std::string& registeredNames);

private:
    static void addParametersFromGroup(std::string_view groupName, JsonValue::Object& properties, JsonValue::Array& required);
    static void addArrayGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, JsonValue::Object& properties);
    static void addSingleGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, JsonValue::Object& properties, JsonValue::Array& required);
    
    // Specific tool helpers
    static void addReadReportSchema(JsonValue::Object& properties, JsonValue::Array& required);
    static void addControlSchema(JsonValue::Object& properties, JsonValue::Array& required);
    static void addManageEnginesSchema(JsonValue::Object& properties, JsonValue::Array& required, const std::string& registeredNames);
    static void addStandardTaskSchema(const ToolInfo& info, JsonValue::Object& properties, JsonValue::Array& required, const std::string& registeredNames);
};

} // namespace QaplaTester::Mcp
