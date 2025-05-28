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
    string key = normalize(name);
    definitions[key] = { description, isRequired, defaultValue, type };
}

void CliSettingsManager::registerGroup(const std::string& groupName,
    const std::string& groupDescription,
    const std::unordered_map<std::string, SettingDefinition>& keys) 
{
    std::string key = normalize(groupName);
    groupDefs[key] = GroupDefinition{ groupDescription, keys };
}

void CliSettingsManager::parseCommandLine(int argc, char** argv) {
    parseGlobalParameters(argc, argv);
    parseGroupedParameters(argc, argv);
}

void CliSettingsManager::parseGlobalParameters(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--help") {
            showHelp();
            exit(0);
        }
        auto eqPos = arg.find('=');
        if (arg.rfind("--", 0) == 0 && eqPos != string::npos) {
            string name = arg.substr(2, eqPos - 2);
            string value = arg.substr(eqPos + 1);
            string key = normalize(name);
            auto defIt = definitions.find(key);
            if (defIt != definitions.end()) {
                values[key] = parseValue(value, defIt->second);
            }
            else {
				throw std::runtime_error("Unknown parameter: " + name);
            }
        }
        else {
			throw std::runtime_error("Invalid argument format: " + arg);

        }
    }

    for (const auto& [key, def] : definitions) {
        if (values.find(key) != values.end())
            continue;

		// Required without Default -> Input required
        if (def.isRequired && def.defaultValue.valueless_by_exception()) {
            std::string input;
            std::cout << key << " (required): ";
            std::getline(std::cin, input);
            values[key] = parseValue(input, def);
            continue;
        }

		// Optional with Default -> set directly, no input needed
        if (!def.isRequired && !def.defaultValue.valueless_by_exception()) {
            values[key] = def.defaultValue;
            continue;
        }

		// Required with Default -> Input with default suggestion
        if (def.isRequired && !def.defaultValue.valueless_by_exception()) {
            std::string inputPrompt = key + " (required, default: ";
            std::visit([&inputPrompt](auto&& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, int>)
                    inputPrompt += std::to_string(v);
                else
                    inputPrompt += v;
                }, def.defaultValue);
            inputPrompt += "): ";

            std::string input;
            std::cout << inputPrompt;
            std::getline(std::cin, input);
            values[key] = input.empty() ? def.defaultValue : parseValue(input, def);
            continue;
        }

		throw std::runtime_error("Invalid setting: optional parameter '" + key + "' without default value.");
    }
}

void CliSettingsManager::parseGroupedParameters(int argc, char** argv) {
    std::string currentGroup;
    ValueMap currentValues;

    auto flushGroup = [&]() {
        if (currentGroup.empty()) return;
        auto defIt = groupDefs.find(currentGroup);
        if (defIt == groupDefs.end()) throw std::runtime_error("Unknown group: " + currentGroup);

        const auto& groupMeta = defIt->second;

        for (const auto& [key, def] : groupMeta.keys) {
            if (currentValues.find(key) != currentValues.end()) continue;
            if (def.isRequired) throw std::runtime_error("Missing required subparameter '" + key + "' in group '" + currentGroup + "'");
            if (!def.defaultValue.valueless_by_exception())
                currentValues[key] = def.defaultValue;
        }

        groupedValues[currentGroup].emplace_back(std::move(currentValues));
        currentValues.clear();
        };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--", 0) != 0) continue;

        std::string raw = arg.substr(2);
        auto eqPos = raw.find('=');
        std::string name = eqPos != std::string::npos ? raw.substr(0, eqPos) : raw;
        std::string val = eqPos != std::string::npos ? raw.substr(eqPos + 1) : "";

        std::string norm = normalize(name);

        if (groupDefs.find(norm) != groupDefs.end()) {
            flushGroup();
            currentGroup = norm;
            continue;
        }

        if (currentGroup.empty()) continue;

        auto groupDefIt = groupDefs.find(currentGroup);
        if (groupDefIt == groupDefs.end()) continue;

        const auto& keys = groupDefIt->second.keys;
        auto keyIt = keys.find(name);
        if (keyIt == keys.end()) throw std::runtime_error("Unknown subparameter '" + name + "' in group '" + currentGroup + "'");

        currentValues[name] = parseValue(val, keyIt->second);
    }

    flushGroup();
}



string CliSettingsManager::normalize(const string& name) {
    string lower = name;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

void CliSettingsManager::showHelp() {
    constexpr int nameWidth = 24;

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
