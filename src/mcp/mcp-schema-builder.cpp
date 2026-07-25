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

#include "mcp-schema-builder.h"
#include "../cli/settings-definitions.h"
#include "../base-elements/oss-tools.h"
#include <format>

namespace QaplaTester::Mcp {

namespace {

    std::string formatDefaultValue(const std::optional<Settings::Value>& value) {
        if (!value.has_value()) {
            return "";
        }
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return arg.empty() ? "" : std::format("(Default: '{}')", arg);
            } else if constexpr (std::is_same_v<T, bool>) {
                return std::format("(Default: {})", arg ? "true" : "false");
            } else {
                return std::format("(Default: {})", arg);
            }
        }, *value);
    }

    Json::JsonValue createProperty(const std::string& type, const std::string& description, const std::string& defaultValStr = "") {
        auto prop = Json::JsonValue::object();
        prop["type"] = type;

        std::string fullDesc = description;
        if (!defaultValStr.empty()) {
            if (!fullDesc.empty() && fullDesc.back() != ' ') {
                fullDesc += " ";
            }
            fullDesc += defaultValStr;
        }
        prop["description"] = fullDesc;
        return prop;
    }

    std::string getJsonType(Settings::ValueType type) {
        switch (type) {
            case Settings::ValueType::Bool: return "boolean";
            case Settings::ValueType::Int:
            case Settings::ValueType::UInt: return "integer";
            case Settings::ValueType::Float: return "number";
            default: return "string";
        }
    }
}

Json::JsonValue McpSchemaBuilder::createInputSchema(const ToolInfo& info, const std::string& registeredNames) {
    auto inputSchema = Json::JsonValue::object();
    inputSchema["type"] = "object";

    auto& properties = inputSchema["properties"] = Json::JsonValue::object();
    auto required = Json::JsonValue::array();

    if (info.name == "read_report") {
        addReadReportSchema(properties, required);
    } else if (info.name == "control") {
        addControlSchema(properties, required);
    } else if (info.name == "manage_engines") {
        addManageEnginesSchema(properties, required, registeredNames);
    } else if (info.name == "set_logging" || info.name == "adjudicate" || info.name == "list_settings") {
        addNonTaskSchema(info, properties);
    } else {
        addStandardTaskSchema(info, properties, required, registeredNames);
    }

    if (required.size() > 0) {
        inputSchema["required"] = std::move(required);
    }
    return inputSchema;
}

void McpSchemaBuilder::addParametersFromGroup(std::string_view groupName, Json::JsonValue& properties) {
    const auto& groupDefs = Settings::Manager::instance().getGroupDefinitions();
    const auto it = groupDefs.find(std::string(groupName));
    if (it == groupDefs.end()) {
        return;
    }

    if (!it->second.unique) {
        addArrayGroupSchema(std::string(groupName), it->second, properties);
    } else {
        addSingleGroupSchema(std::string(groupName), it->second, properties);
    }
}

void McpSchemaBuilder::addReadReportSchema(Json::JsonValue& properties, Json::JsonValue& required) {
    properties["uri"] = createProperty("string", "The URI or filename of the report to read (e.g. qapla://reports/sprt/report.log)");
    required.push_back("uri");
}

void McpSchemaBuilder::addControlSchema(Json::JsonValue& properties, Json::JsonValue& required) {
    auto command = createProperty("string", "The operation to perform.");
    auto& commandEnum = command["enum"] = Json::JsonValue::array();
    for (const char* option : {"status", "set_concurrency", "stop", "stop_nice",
                               "cancel_job", "clear_queue", "list_results", "clear_results"}) {
        commandEnum.push_back(option);
    }
    properties["command"] = std::move(command);
    properties["value"] = createProperty("integer", "Value for the command (e.g. concurrency level).");
    properties["job_id"] = createProperty("string", "Job id for queue control commands like cancel_job.");
    required.push_back("command");
}

void McpSchemaBuilder::addManageEnginesSchema(Json::JsonValue& properties, Json::JsonValue& required, const std::string& registeredNames) {
    auto command = createProperty("string", "The operation to perform on engines.");
    auto& commandEnum = command["enum"] = Json::JsonValue::array();
    for (const char* option : {"list", "details", "add", "copy", "update", "delete", "update_all"}) {
        commandEnum.push_back(option);
    }
    properties["command"] = std::move(command);
    required.push_back("command");

    properties["engine_name"] = createProperty("string", std::format("Primary engine name (Available: {})", registeredNames));

    // Manually add engine parameters with engine_ prefix since the "engine" group is not unique
    const auto allEngineKeys = Settings::getEngineKeys();
    for (const auto& [key, def] : allEngineKeys) {
        if (def.isHidden || key == "id" || key == "name" || key == "conf" ||
            key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }

        std::string desc = def.longDescription.empty() ? def.description : def.longDescription;
        properties[std::format("engine_{}", key)] = createProperty(getJsonType(def.type), desc, formatDefaultValue(def.defaultValue));
    }

    properties["engine_copyName"] = createProperty("string",
        "Target name when copying an engine. When using 'copy', you can concurrently specify any other engine parameter "
        "(e.g. engine_tc, engine_option_Hash, etc.) to immediately override these settings in the new copy. Do not copy and then update, this is unnecessary.");
    properties["engine_option_name"] = createProperty("string",
        "Set one or more UCI options. You can pass multiple arguments matching "
        "the pattern 'engine_option_<Name>' in a single call to update several options simultaneously. "
        "Syntax: engine_option_<OptionName>=<Value>. Example: engine_option_Hash=128."
        "Use the 'details' command to list available options for a specific engine.");
}

void McpSchemaBuilder::addNonTaskSchema(const ToolInfo& info, Json::JsonValue& properties) {
    for (const auto& group : info.groups) {
        addParametersFromGroup(group, properties);
    }
}

void McpSchemaBuilder::addStandardTaskSchema(const ToolInfo& info, Json::JsonValue& properties, Json::JsonValue& required, const std::string& registeredNames) {
    properties["engines"] = createProperty("string", std::format("Comma separated list of engine names from the registry (Available: {}).", registeredNames));
    required.push_back("engines");

    if (info.name == "sprt" || info.name == "tournament" || info.name == "epd" ||
        info.name == "spsa" || info.name == "clop" || info.name == "test") {
        properties["job_intent"] = createProperty(
            "string",
            "Short and precise purpose of this queued job. Include key specifics like tested parameter/value and expected comparison goal.");
        required.push_back("job_intent");
    }

    addGlobalParameterSchema("concurrency", properties);
    addGlobalParameterSchema("rapid", properties);

    properties["mcp_background"] = createProperty("boolean", "If true, starts the task in background and returns immediately. Use 'control' tool to monitor.");

    if (info.name == "sprt" || info.name == "tournament") {
         properties["resume"] = createProperty("boolean", "If true, resumes sending results to the last used file. If false (default), creates a new timestamped file.");
    }

    if (info.name == "sprt" || info.name == "tournament" || info.name == "spsa" || info.name == "clop") {
        properties["engine_tc"] = createProperty("string", "Set the time control (engine_tc) for all participating engines. This also updates the engine configuration until the service is restarted.");
    }

    for (const auto& group : info.groups) {
        addParametersFromGroup(group, properties);
    }
}

void McpSchemaBuilder::addArrayGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, Json::JsonValue& properties) {
    auto arrayProp = Json::JsonValue::object();
    arrayProp["type"] = "array";

    auto items = Json::JsonValue::object();
    items["type"] = "object";

    auto& itemProperties = items["properties"] = Json::JsonValue::object();
    for (const auto& [key, keyDef] : def.keys) {
        if (keyDef.isHidden || key == "id" || key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }

        std::string desc = keyDef.longDescription.empty() ? keyDef.description : keyDef.longDescription;
        itemProperties[key] = createProperty(getJsonType(keyDef.type), desc, formatDefaultValue(keyDef.defaultValue));
    }

    arrayProp["items"] = std::move(items);
    arrayProp["description"] = def.longDescription.empty() ? def.description : def.longDescription;

    properties[groupName] = std::move(arrayProp);
}

void McpSchemaBuilder::addSingleGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, Json::JsonValue& properties) {
    for (const auto& [key, keyDef] : def.keys) {
        if (key == "file" && (groupName == "sprt" || groupName == "tournament")) {
            continue;
        }

        if (groupName == "logging" && key == "trace") {
            continue;
        }

        if (keyDef.isHidden || key == "id" || key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }

        std::string paramName = std::format("{}_{}", groupName, key);
        std::string desc = keyDef.longDescription.empty() ? keyDef.description : keyDef.longDescription;

        properties[paramName] = createProperty(getJsonType(keyDef.type), desc, formatDefaultValue(keyDef.defaultValue));
    }
}

void McpSchemaBuilder::addGlobalParameterSchema(const std::string& key, Json::JsonValue& properties) {
    const auto& globalDefs = Settings::Manager::instance().getDefinitions();
    const auto it = globalDefs.find(key);
    if (it == globalDefs.end()) {
        return;
    }
    const auto& def = it->second;

    std::string desc = def.longDescription.empty() ? def.description : def.longDescription;

    if (key == "concurrency") {
        const int cores = QaplaHelpers::getPhysicalCoreCount();
        if (cores > 0) {
            desc += std::format(" (Detected physical cores: {})", cores);
        }
    }

    properties[key] = createProperty(getJsonType(def.type), desc, formatDefaultValue(def.defaultValue));
}

} // namespace QaplaTester::Mcp
