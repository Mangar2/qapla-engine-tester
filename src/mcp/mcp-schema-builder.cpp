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

    JsonValue::Object createProperty(const std::string& type, const std::string& description, const std::string& defaultValStr = "") {
        JsonValue::Object prop;
        prop["type"] = JsonValue{ .data = type };
        
        std::string fullDesc = description;
        if (!defaultValStr.empty()) {
            if (!fullDesc.empty() && fullDesc.back() != ' ') {
                fullDesc += " ";
            }
            fullDesc += defaultValStr;
        }
        prop["description"] = JsonValue{ .data = fullDesc };
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

JsonValue::Object McpSchemaBuilder::createInputSchema(const ToolInfo& info, const std::string& registeredNames) {
    JsonValue::Object inputSchema;
    inputSchema["type"] = JsonValue{ .data = std::string("object") };
    
    JsonValue::Object properties;
    JsonValue::Array required;
    
    if (info.name == "read_report") {
        addReadReportSchema(properties, required);
    } else if (info.name == "control") {
        addControlSchema(properties, required);
    } else if (info.name == "manage_engines") {
        addManageEnginesSchema(properties, required, registeredNames);
    } else {
        addStandardTaskSchema(info, properties, required, registeredNames);
    }
    
    inputSchema["properties"] = JsonValue{ .data = properties };
    if (!required.empty()) {
        inputSchema["required"] = JsonValue{ .data = required };
    }
    return inputSchema;
}

void McpSchemaBuilder::addParametersFromGroup(std::string_view groupName, JsonValue::Object& properties, JsonValue::Array& required) {
    const auto& groupDefs = Settings::Manager::instance().getGroupDefinitions();
    const auto it = groupDefs.find(std::string(groupName));
    if (it == groupDefs.end()) {
        return;
    }

    if (!it->second.unique) {
        addArrayGroupSchema(std::string(groupName), it->second, properties);
    } else {
        addSingleGroupSchema(std::string(groupName), it->second, properties, required);
    }
}

void McpSchemaBuilder::addReadReportSchema(JsonValue::Object& properties, JsonValue::Array& required) {
    properties["uri"] = JsonValue{ .data = createProperty("string", "The URI or filename of the report to read (e.g. qapla://reports/sprt/report.log)") };
    required.push_back(JsonValue{ .data = std::string("uri") });
}

void McpSchemaBuilder::addControlSchema(JsonValue::Object& properties, JsonValue::Array& required) {
    JsonValue::Object command = createProperty("string", "The operation to perform.");
    command["enum"] = JsonValue{ .data = JsonValue::Array{ 
        JsonValue{ .data = std::string("status") }, 
        JsonValue{ .data = std::string("set_concurrency") }, 
        JsonValue{ .data = std::string("stop") }, 
        JsonValue{ .data = std::string("stop_nice") },
        JsonValue{ .data = std::string("cancel_job") },
        JsonValue{ .data = std::string("clear_queue") },
        JsonValue{ .data = std::string("list_results") },
        JsonValue{ .data = std::string("clear_results") }
    } };
    properties["command"] = JsonValue{ .data = command };
    properties["value"] = JsonValue{ .data = createProperty("integer", "Value for the command (e.g. concurrency level).") };
    properties["job_id"] = JsonValue{ .data = createProperty("string", "Job id for queue control commands like cancel_job.") };
    required.push_back(JsonValue{ .data = std::string("command") });
}

void McpSchemaBuilder::addManageEnginesSchema(JsonValue::Object& properties, JsonValue::Array& required, const std::string& registeredNames) {
    JsonValue::Object command = createProperty("string", "The operation to perform on engines.");
    command["enum"] = JsonValue{ .data = JsonValue::Array{ 
        JsonValue{ .data = std::string("list") }, 
        JsonValue{ .data = std::string("details") }, 
        JsonValue{ .data = std::string("add") }, 
        JsonValue{ .data = std::string("copy") }, 
        JsonValue{ .data = std::string("update") }, 
        JsonValue{ .data = std::string("delete") }, 
        JsonValue{ .data = std::string("update_all") } 
    } };
    properties["command"] = JsonValue{ .data = command };
    required.push_back(JsonValue{ .data = std::string("command") });

    properties["engine_name"] = JsonValue{ .data = createProperty("string", std::format("Primary engine name (Available: {})", registeredNames)) };

    // Manually add engine parameters with engine_ prefix since the "engine" group is not unique
    const auto allEngineKeys = Settings::getEngineKeys();
    for (const auto& [key, def] : allEngineKeys) {
        if (def.isHidden || key == "id" || key == "name" || key == "conf" ||
            key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }
        
        std::string desc = def.longDescription.empty() ? def.description : def.longDescription;
        properties[std::format("engine_{}", key)] = JsonValue{ .data = createProperty(getJsonType(def.type), desc, formatDefaultValue(def.defaultValue)) };
    }

    properties["engine_copyName"] = JsonValue{ .data = createProperty("string", 
        "Target name when copying an engine. When using 'copy', you can concurrently specify any other engine parameter "
        "(e.g. engine_tc, engine_option_Hash, etc.) to immediately override these settings in the new copy. Do not copy and then update, this is unnecessary.") };
    properties["engine_option_<name>"] = JsonValue{ .data = createProperty("string", 
        "Set one or more UCI options. You can pass multiple arguments matching "
        "the pattern 'engine_option_<Name>' in a single call to update several options simultaneously. "
        "Syntax: engine_option_<OptionName>=<Value>. Example: engine_option_Hash=128."
        "Use the 'details' command to list available options for a specific engine.") };
}
void McpSchemaBuilder::addStandardTaskSchema(const ToolInfo& info, JsonValue::Object& properties, JsonValue::Array& required, const std::string& registeredNames) {
    properties["engines"] = JsonValue{ .data = createProperty("string", std::format("Comma separated list of engine names from the registry (Available: {}).", registeredNames)) };
    required.push_back(JsonValue{ .data = std::string("engines") });

    if (info.name == "sprt") {
        properties["job_intent"] = JsonValue{ .data = createProperty(
            "string",
            "Short and precise purpose of this SPRT job. Include key specifics like tested parameter/value and expected comparison goal.") };
        required.push_back(JsonValue{ .data = std::string("job_intent") });
    }

    addGlobalParameterSchema("concurrency", properties);
    addGlobalParameterSchema("rapid", properties);

    properties["mcp_background"] = JsonValue{ .data = createProperty("boolean", "If true, starts the task in background and returns immediately. Use 'control' tool to monitor.") };

    if (info.name == "sprt" || info.name == "tournament") {
         properties["resume"] = JsonValue{ .data = createProperty("boolean", "If true, resumes sending results to the last used file. If false (default), creates a new timestamped file.") };
    }

    if (info.name == "sprt" || info.name == "tournament" || info.name == "epd" || info.name == "spsa") {
        properties["engine_tc"] = JsonValue{ .data = createProperty("string", "Set the time control (engine_tc) for all participating engines. This also updates the engine configuration until the service is restarted.") };
    }

    for (const auto& group : info.groups) {
        addParametersFromGroup(group, properties, required);
    }
}

void McpSchemaBuilder::addArrayGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, JsonValue::Object& properties) {
    JsonValue::Object arrayProp;
    arrayProp["type"] = JsonValue{ .data = std::string("array") };
    
    JsonValue::Object items;
    items["type"] = JsonValue{ .data = std::string("object") };
    
    JsonValue::Object itemProperties;
    JsonValue::Array itemRequired;
    
    for (const auto& [key, keyDef] : def.keys) {
        if (keyDef.isHidden || key == "id" || key.find('[') != std::string::npos || key.find(']') != std::string::npos) {
            continue;
        }
        
        std::string desc = keyDef.longDescription.empty() ? keyDef.description : keyDef.longDescription;
        itemProperties[key] = JsonValue{ .data = createProperty(getJsonType(keyDef.type), desc, formatDefaultValue(keyDef.defaultValue)) };
        
        if (keyDef.isRequired) {
            bool canBeGlobal = (keyDef.type == Settings::ValueType::PathExists || 
                              keyDef.type == Settings::ValueType::ValidateOutputPath);
            
            if (!canBeGlobal) {
                itemRequired.push_back(JsonValue{ .data = key });
            }
        }
    }
    
    items["properties"] = JsonValue{ .data = itemProperties };
    if (!itemRequired.empty()) {
        items["required"] = JsonValue{ .data = itemRequired };
    }
    arrayProp["items"] = JsonValue{ .data = items };

    std::string groupDesc = def.longDescription.empty() ? def.description : def.longDescription;
    arrayProp["description"] = JsonValue{ .data = groupDesc };
    
    properties[groupName] = JsonValue{ .data = arrayProp };
}

void McpSchemaBuilder::addSingleGroupSchema(const std::string& groupName, const Settings::GroupDefinition& def, JsonValue::Object& properties, JsonValue::Array& required) {
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
        
        properties[paramName] = JsonValue{ .data = createProperty(getJsonType(keyDef.type), desc, formatDefaultValue(keyDef.defaultValue)) };
        
        if (keyDef.isRequired) {
            bool canBeGlobal = (keyDef.type == Settings::ValueType::PathExists || 
                              keyDef.type == Settings::ValueType::ValidateOutputPath);
            
            if (!canBeGlobal) {
                required.push_back(JsonValue{ .data = paramName });
            }
        }
    }
}

void McpSchemaBuilder::addGlobalParameterSchema(const std::string& key, JsonValue::Object& properties) {
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

    properties[key] = JsonValue{ .data = createProperty(getJsonType(def.type), desc, formatDefaultValue(def.defaultValue)) };
}

} // namespace QaplaTester::Mcp
