#include "settings-reporter.h"
#include <format>
#include <algorithm>
#include <vector>

namespace QaplaTester::Mcp {

std::string SettingsReporter::generateReport(std::optional<std::vector<std::string>> groups, std::optional<std::vector<Column>> columns) {
    std::string report;

    // Default columns if not provided
    std::vector<Column> cols;
    if (columns.has_value()) {
        cols = columns.value();
    } else {
        cols = {Column::Name, Column::FullName, Column::Value, Column::Default, Column::Required, Column::Description};
    }

    bool showGlobal = true;
    std::vector<std::string> groupFilter;
    
    if (groups.has_value()) {
        groupFilter = groups.value();
        showGlobal = std::ranges::find(groupFilter, "global") != groupFilter.end();
    }

    if (showGlobal) {
        appendGlobalSettingsReport(report, cols);
    }

    if (!groups.has_value()) {
        groupFilter = {"openings", "epd", "sprt", "tournament", "spsa", "pgnoutput", "test", "draw", "resign", "logging"};
    }

    appendGroupSettingsReport(report, groupFilter, cols);

    return report;
}

std::string SettingsReporter::formatSettingValue(const std::variant<std::string, int, unsigned int, bool, double>& v) {
    if (std::holds_alternative<bool>(v)) {
        return std::get<bool>(v) ? "true" : "false";
    }
    return Settings::to_string(v);
}

std::string SettingsReporter::getColumnHeader(Column col) {
    switch (col) {
        case Column::Name: return "Name";
        case Column::FullName: return "Full Name";
        case Column::Value: return "Value";
        case Column::Default: return "Default";
        case Column::Required: return "Required";
        case Column::Description: return "Description";
    }
    return "";
}

std::string SettingsReporter::getColumnValue(Column col, const std::string& name, const std::string& fullName, 
                                            const std::string& value, const std::string& defaultValue, 
                                            bool required, const std::string& description) {
    switch (col) {
        case Column::Name: return name;
        case Column::FullName: return std::format("**{}**", fullName);
        case Column::Value: return value;
        case Column::Default: return defaultValue;
        case Column::Required: return required ? "Yes" : "No";
        case Column::Description: return description;
    }
    return "";
}

std::string SettingsReporter::getGlobalValue(QaplaTester::Settings::Manager& manager, const std::string& name, const QaplaTester::Settings::ParameterDefinition& def) {
     try {
         if (def.type == Settings::ValueType::String || 
             def.type == Settings::ValueType::PathExists || 
             def.type == Settings::ValueType::ValidateOutputPath) {
             return manager.get<std::string>(name);
         }
         if (def.type == Settings::ValueType::Int) {
             return std::to_string(manager.get<int>(name));
         } 
         if (def.type == Settings::ValueType::UInt) {
             return std::to_string(manager.get<unsigned int>(name));
         } 
         if (def.type == Settings::ValueType::Float) {
             return std::format("{}", manager.get<double>(name));
         } 
         if (def.type == Settings::ValueType::Bool) {
             return manager.get<bool>(name) ? "true" : "false";
         }
    } catch (...) {
        return "Error";
    }
    return "Error";
}

void SettingsReporter::appendGlobalSettingsReport(std::string& report, const std::vector<Column>& columns) {
    auto& manager = Settings::Manager::instance();

    if (!report.empty()) {
        report += "\n\n";
    }
    report += "Global Settings\n";

    std::vector<std::string> keys;
    for (const auto& [name, _] : manager.getDefinitions()) {
        keys.push_back(name);
    }
    std::ranges::sort(keys);

    const bool simpleFormat = columns.size() <= 2;

    for (const auto& name : keys) {
        const auto& def = manager.getDefinitions().at(name);
        std::string currentVal = getGlobalValue(manager, name, def);

        const bool isSet = manager.isKeyProvided(name);
        if (!isSet && def.isRequired && !def.defaultValue.has_value()) {
            currentVal = "MISSING";
        }

        // Determine key string (Name or FullName)
        std::string keyStr = name;
        for (const auto& col : columns) {
            if (col == Column::FullName) {
                // Global settings usually have same name and fullname
                keyStr = name;
                break;
            }
        }

        appendFormattedRow(report, keyStr, currentVal, name, name, 
            def.defaultValue.has_value() ? formatSettingValue(def.defaultValue.value()) : "-", 
            def.isRequired, def.description, columns, simpleFormat);
    }
}

void SettingsReporter::processGroupSection(
    const std::string& groupName,
    QaplaTester::Settings::Manager& manager,
    std::string& report,
    const std::vector<SettingsReporter::Column>& columns) {
    if (!manager.getGroupDefinitions().contains(groupName)) {
        return;
    }

    report += std::format("\nSettings Group: {}\n", groupName);

    // Use first instance if any (simplification for reporting)
    const std::vector<QaplaTester::Settings::GroupInstance>& instanceList = manager.getGroupInstances(groupName);
    const QaplaTester::Settings::GroupInstance* instance = nullptr;
    if (!instanceList.empty()) {
        instance = instanceList.data();
    }

    // Get keys
    const auto& groupDef = manager.getGroupDefinitions().at(groupName);
    std::vector<std::string> paramKeys;
    for (const auto& [k, v] : groupDef.keys) {
        paramKeys.push_back(k);
    }
    std::ranges::sort(paramKeys);

    const bool simpleFormat = columns.size() <= 2;

    for (const auto& key : paramKeys) {
        const auto& paramDef = groupDef.keys.at(key);
        std::string currentVal = "MISSING";

        const bool isProvided = (instance != nullptr && instance->isKeyProvided(key));

        if (isProvided) {
            if (instance->getValues().contains(key)) {
                currentVal = formatSettingValue(instance->getValues().at(key));
            }
        } else if (paramDef.defaultValue.has_value()) {
            currentVal = formatSettingValue(paramDef.defaultValue.value());
        }

        const std::string fullName = std::format("{}_{}", groupName, key);
        std::string keyStr = key;

        // If FullName column is present, use fullName as key
        for (const auto& c : columns) {
            if (c == Column::FullName) {
                keyStr = fullName;
                break;
            }
        }

        appendFormattedRow(report, keyStr, currentVal, key, fullName, 
             paramDef.defaultValue.has_value() ? formatSettingValue(paramDef.defaultValue.value()) : "-", 
             paramDef.isRequired, paramDef.description, columns, simpleFormat);
    }
}

void SettingsReporter::appendFormattedRow(std::string& report, const std::string& keyStr, 
        const std::string& currentVal, const std::string& name, const std::string& fullName,
        const std::string& defaultVal, bool isRequired, const std::string& description,
        const std::vector<Column>& columns, bool simpleFormat) {
    
    report += std::format("  {}: {}\n", keyStr, currentVal);

    if (!simpleFormat) {
        for (const auto& col : columns) {
            // Skip primary key and value, show others nested
            if (col == Column::Name || col == Column::FullName || col == Column::Value) {
                continue;
            }

            std::string val = getColumnValue(col, name, fullName, currentVal,
                                                 defaultVal,
                                                 isRequired, description);

            // Remove Markdown formatting for plain text output if necessary
            if (col == Column::Required) {
                val = (isRequired ? "Yes" : "No");
            }

            if (col == Column::FullName) {
                // Remove markdown bold
                if (val.size() > 4) {
                    val = val.substr(2, val.size() - 4);
                }
            }

            report += std::format("    {}: {}\n", getColumnHeader(col), val);
        }
    }
}

void SettingsReporter::appendGroupSettingsReport(std::string& report, const std::vector<std::string>& groupFilter, const std::vector<Column>& columns) {
    auto& manager = QaplaTester::Settings::Manager::instance();

    for (const auto& groupName : groupFilter) {
        if (groupName == "global") {
            continue;
        }
        processGroupSection(groupName, manager, report, columns);
    }
}


} // namespace QaplaTester::Mcp
