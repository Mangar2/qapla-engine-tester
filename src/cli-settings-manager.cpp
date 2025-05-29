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

#include "cli-settings-manager.h"

#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>

using namespace std;

void CliSettingsManager::registerSetting(const string& name,
    const string& description,
    bool isRequired,
    Value defaultValue,
    ValueType type) {
    string key = toLowercase(name);
    definitions[key] = { description, isRequired, defaultValue, type };
}

void CliSettingsManager::registerGroup(const std::string& groupName,
    const std::string& groupDescription,
    const std::unordered_map<std::string, SettingDefinition>& keys) 
{
    std::string key = toLowercase(groupName);
    groupDefs[key] = GroupDefinition{ groupDescription, keys };
}

const std::vector<CliSettingsManager::ValueMap>& CliSettingsManager::getGroup(const std::string& groupName) {
    std::string key = toLowercase(groupName);
    auto it = groupedValues.find(key);
    if (it == groupedValues.end() || it->second.empty()) {
        throw std::runtime_error("Unknown or empty group: " + groupName);
    }
    return it->second; 
}

void CliSettingsManager::parseCommandLine(int argc, char** argv) {
    int index = 1;

    while (index < argc) {
        std::string arg = argv[index];

        if (arg == "--help") {
            showHelp();
            exit(0);
        }

        if (arg.rfind("--", 0) != 0) {
            throw std::runtime_error("Invalid argument format: " + arg);
        }

        std::string name = toLowercase(arg.substr(2));
        if (groupDefs.find(name) != groupDefs.end()) {
            index = parseGroupedParameter(index, argc, argv);
        }
        else {
            index = parseGlobalParameter(index, argc, argv);
        }
    }

    finalizeGlobalParameters();
}

int CliSettingsManager::parseGlobalParameter(int index, int argc, char** argv) {
    std::string arg = argv[index];

    auto eqPos = arg.find('=');
    if (arg.rfind("--", 0) != 0 || eqPos == std::string::npos)
        throw std::runtime_error("Invalid global parameter format: " + arg);

    std::string name = arg.substr(2, eqPos - 2);
    std::string value = arg.substr(eqPos + 1);
    std::string key = toLowercase(name);

    auto it = definitions.find(key);
    if (it == definitions.end())
        throw std::runtime_error("Unknown global parameter: " + name);

    values[key] = parseValue(value, it->second);
    return index + 1;
}

const CliSettingsManager::SettingDefinition* CliSettingsManager::resolveGroupedKey(const GroupDefinition& group, const std::string& name) {
    auto it = group.keys.find(name);
    if (it != group.keys.end()) return &it->second;
    std::string postFix = "[name]";
    for (const auto& [key, def] : group.keys) {
        if (key.ends_with("." + postFix)) {
            std::string prefix = key.substr(0, key.size() - postFix.length());
            if (name.rfind(prefix, 0) == 0) return &def;
        }
    }

    return nullptr;
}

int CliSettingsManager::parseGroupedParameter(int index, int argc, char** argv) {
    std::string groupName = toLowercase(argv[index++]).substr(2);

    auto defIt = groupDefs.find(groupName);
    if (defIt == groupDefs.end())
        throw std::runtime_error("Unknown group: " + groupName);

    const auto& groupMeta = defIt->second;
    ValueMap group;

    while (index < argc) {
        std::string arg = argv[index];

        if (arg.rfind("--", 0) == 0) break; // nächste Gruppe oder globaler Parameter

        auto eqPos = arg.find('=');
        if (eqPos == std::string::npos)
            throw std::runtime_error("Invalid group parameter format: " + arg);

        std::string name = arg.substr(0, eqPos);
        std::string value = arg.substr(eqPos + 1);

        const SettingDefinition* def = resolveGroupedKey(groupMeta, name);
        if (!def)
            throw std::runtime_error("Unknown parameter '" + name + "' in group '" + groupName + "'");
        group[name] = parseValue(value, *def);

        ++index;
    }

    // apply defaults
    for (const auto& [key, def] : groupMeta.keys) {
        if (key.ends_with(".[name]")) continue;
        if (group.find(key) != group.end()) continue;
        if (def.isRequired)
            throw std::runtime_error("Missing required parameter '" + key + "' in group '" + groupName + "'");
        if (!def.defaultValue.valueless_by_exception())
            group[key] = def.defaultValue;
    }

    groupedValues[groupName].push_back(std::move(group));
    return index;
}

void CliSettingsManager::finalizeGlobalParameters() {
    for (const auto& [key, def] : definitions) {
        if (values.contains(key)) continue;

        if (def.isRequired && def.defaultValue.valueless_by_exception()) {
            std::string input;
            std::cout << key << " (required): ";
            std::getline(std::cin, input);
            values[key] = parseValue(input, def);
        }
        else if (!def.isRequired && !def.defaultValue.valueless_by_exception()) {
            values[key] = def.defaultValue;
        }
        else if (def.isRequired) {
            values[key] = def.defaultValue;
        }
    }
}

string CliSettingsManager::toLowercase(const string& name) {
    string lower = name;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

void CliSettingsManager::showHelp() {
    constexpr int nameWidth = 30;

    std::cout << "Available options:\n";
    for (const auto& [key, def] : definitions) {
        std::ostringstream line;
        line << "  --" << key << "=";

        std::string typeStr = (def.type == ValueType::Int) ? "<int>" :
            (def.type == ValueType::Bool) ? "<bool>" :
            (def.type == ValueType::PathExists) ? "<path>" : "<string>";

        line << typeStr;
        std::cout << std::left << std::setw(nameWidth) << line.str();

        std::cout << def.description;
        if (def.isRequired) std::cout << " [required]";
        else {
            std::cout << " (default: ";
            std::visit([](auto&& v) { std::cout << v; }, def.defaultValue);
            std::cout << ")";
        }
        std::cout << "\n";
    }

    for (const auto& [group, def] : groupDefs) {
        std::ostringstream header;
        header << "  --" << group << " ...";

        std::cout << "\n" << std::left << std::setw(nameWidth) << header.str();
        std::cout << def.description << "\n";

        for (const auto& [param, meta] : def.keys) {
            std::ostringstream line;
            line << "    " << param << "=";

            std::string typeStr = (meta.type == ValueType::Int) ? "<int>" :
                (meta.type == ValueType::Bool) ? "<bool>" :
                (meta.type == ValueType::PathExists) ? "<path>" : "<string>";

            line << typeStr;
            std::cout << std::left << std::setw(nameWidth) << line.str();

            std::cout << meta.description;
            if (meta.isRequired) std::cout << " [required]";
            else {
                std::cout << " (default: ";
                std::visit([](auto&& v) { std::cout << v; }, meta.defaultValue);
                std::cout << ")";
            }
            std::cout << "\n";
        }
    }
}


CliSettingsManager::Value CliSettingsManager::parseValue(const string& input, const SettingDefinition& def) {
    if (def.type == ValueType::Int) {
        try {
            return stoi(input);
        }
        catch (...) {
			throw std::runtime_error("Invalid integer: " + input);
        }
    }

    if (def.type == ValueType::PathExists) {
        if (!filesystem::exists(input)) {
			throw std::runtime_error("Path does not exist: " + input);
        }
    }

    return input;
}
