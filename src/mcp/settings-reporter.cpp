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
    auto formatDefValueGlob = [&](const std::optional<Settings::Value>& v) -> std::string {
        if (!v.has_value()) {
            return "-";
        }
        return formatSettingValue(v.value());
    };

    if (!report.empty()) {
        report += "\n\n";
    }
    report += "# Global Settings\n\n";

    // Build Header
    report += "|";
    for (const auto& col : columns) {
        report += " " + getColumnHeader(col) + " |";
    }
    report += "\n|";
    for (size_t i = 0; i < columns.size(); ++i) {
        report += "---|";
    }
    report += "\n";
    
    std::vector<std::string> keys;
    for(const auto& [name, _] : manager.getDefinitions()) {
        keys.push_back(name);
    }
    std::ranges::sort(keys);

    for (const auto& name : keys) {
        const auto& def = manager.getDefinitions().at(name);
        std::string currentVal = "-";
        bool isSet = manager.isKeyProvided(name);
        
        auto& mutManager = const_cast<Settings::Manager&>(manager);
        currentVal = getGlobalValue(mutManager, name, def);

        std::string status = "OK";
        if (def.isRequired && !isSet && !def.defaultValue.has_value()) {
            status = "MISSING";
        } else if (!isSet) {
            status = "Default";
        } else {
            status = "Set";
        }

        if (status == "MISSING") {
            currentVal = "**MISSING**";
        }

        report += "|";
        for (const auto& col : columns) {
            report += " " + getColumnValue(col, name, name, currentVal, formatDefValueGlob(def.defaultValue), def.isRequired, def.description) + " |";
        }
        report += "\n";
    }
}

void SettingsReporter::processGroupSection(
    const std::string& groupName,
    QaplaTester::Settings::Manager& manager,
    std::string& report,
    const std::vector<SettingsReporter::Column>& columns,
    const std::function<std::string(const std::optional<QaplaTester::Settings::Value>&)>& formatDefValueLoc
) {
    if (!manager.getGroupDefinitions().contains(groupName)) {
        return;
    }
    
    report += std::format("\n## Group: {}\n\n", groupName);
    
    // Build Header
    report += "|";
    for (const auto& col : columns) {
        report += " " + SettingsReporter::getColumnHeader(col) + " |";
    }
    report += "\n|";
    for (size_t i = 0; i < columns.size(); ++i) {
        report += "---|";
    }
    report += "\n";

    const auto& groupDef = manager.getGroupDefinitions().at(groupName);
    auto instanceList = manager.getGroupInstances(groupName);
    
    // Use first instance if any
    const QaplaTester::Settings::GroupInstance* instance = nullptr;
    if (!instanceList.empty()) {
        instance = instanceList.data();
    }

    std::vector<std::string> paramKeys = groupDef.getKeyNames();
    std::ranges::sort(paramKeys);

    for (const auto& key : paramKeys) {
        const auto& paramDef = groupDef.keys.at(key);
        std::string currentVal = "**MISSING**";
        
        bool isProvided = (instance != nullptr && instance->isKeyProvided(key));
        
        if (isProvided) {
             const auto& valMap = instance->getValues();
             if (valMap.contains(key)) {
                 currentVal = formatSettingValue(valMap.at(key));
             }
        } else if (paramDef.defaultValue.has_value()) {
             currentVal = formatSettingValue(paramDef.defaultValue.value());
        } 
        
        std::string fullName = std::format("{}_{}", groupName, key);

        report += "|";
        for (const auto& col : columns) {
            report += " " + SettingsReporter::getColumnValue(col, key, fullName, currentVal, formatDefValueLoc(paramDef.defaultValue), paramDef.isRequired, paramDef.description) + " |";
        }
        report += "\n";
    }
}

void SettingsReporter::appendGroupSettingsReport(std::string& report, const std::vector<std::string>& groupFilter, const std::vector<Column>& columns) {
    auto& manager = QaplaTester::Settings::Manager::instance();
    auto formatDefValueLoc = [&](const std::optional<QaplaTester::Settings::Value>& v) -> std::string {
        if (!v.has_value()) {
            return "-";
        }
        return formatSettingValue(v.value());
    };

    if (!report.empty()) {
        report += "\n";
    }
    report += "\n# Group Settings\n";
    
    for (const auto& groupName : groupFilter) {
        if (groupName == "global") {
            continue; 
        }
        processGroupSection(groupName, manager, report, columns, formatDefValueLoc);
    }
}


} // namespace QaplaTester::Mcp
