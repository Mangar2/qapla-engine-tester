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

namespace CliSettings {

    void Manager::registerSetting(const std::string& name,
        const std::string& description,
        bool isRequired,
        Value defaultValue,
        ValueType type) {
        std::string key = to_lowercase(name);
        definitions_[key] = { description, isRequired, defaultValue, type };
    }

    void Manager::registerGroup(const std::string& groupName,
        const std::string& groupDescription,
        bool unique,
        const std::unordered_map<std::string, Definition>& keys)
    {
        std::string key = to_lowercase(groupName);
        groupDefs_[key] = GroupDefinition{ groupDescription, unique, keys };
    }

    const GroupInstances Manager::getGroupInstances(const std::string& groupName) {
        std::string key = to_lowercase(groupName);
        auto it = groupInstances_.find(key);
        if (it == groupInstances_.end() || it->second.empty()) {
			return std::vector<GroupInstance>();
        }
        return it->second;
    }

    const std::optional<GroupInstance> Manager::getGroupInstance(const std::string& groupName) {
        std::string key = to_lowercase(groupName);
        auto it = groupInstances_.find(key);
        if (it == groupInstances_.end() || it->second.empty()) {
			return std::nullopt; 
        }
		if (it->second.size() == 0) {
			return std::nullopt; 
		}
        return it->second[0];
    }

    void Manager::parseCommandLine(int argc, char** argv) {
        int index = 1;

        while (index < argc) {
            std::string arg = to_lowercase(argv[index]);

            if (arg == "--help") {
                showHelp();
                exit(0);
            }

            if (arg.rfind("--", 0) != 0) {
                throw std::runtime_error("Invalid argument format: " + arg);
            }

            std::string name = arg.substr(2);
            if (groupDefs_.find(name) != groupDefs_.end()) {
                index = parseGroupedParameter(index, argc, argv);
            }
            else {
                index = parseGlobalParameter(index, argc, argv);
            }
        }

        finalizeGlobalParameters();
    }

    int Manager::parseGlobalParameter(int index, int argc, char** argv) {
        std::string arg = argv[index];

        auto eqPos = arg.find('=');
        if (arg.rfind("--", 0) != 0 || eqPos == std::string::npos)
            throw std::runtime_error("Invalid global parameter format: " + arg);

        std::string name = arg.substr(2, eqPos - 2);
        std::string value = arg.substr(eqPos + 1);
        std::string key = to_lowercase(name);

        auto it = definitions_.find(key);
        if (it == definitions_.end())
            throw std::runtime_error("Unknown global parameter: " + name);

        values_[key] = parseValue(value, it->second);
        return index + 1;
    }

    const Definition* Manager::resolveGroupedKey(const GroupDefinition& group, const std::string& name) {
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

    int Manager::parseGroupedParameter(int index, int argc, char** argv) {
        std::string groupName = to_lowercase(argv[index++]).substr(2);

        auto defIt = groupDefs_.find(groupName);
        if (defIt == groupDefs_.end())
            throw std::runtime_error("Unknown group: " + groupName);

        const auto& groupDefinition = defIt->second;
        ValueMap group;

		if (groupDefinition.unique && groupInstances_.contains(groupName)) {
			throw std::runtime_error("Group '" + groupName + "' can only be defined once.");
		}

        while (index < argc) {
            std::string arg = to_lowercase(argv[index]);

            if (arg.rfind("--", 0) == 0) break;

            auto eqPos = arg.find('=');
            if (eqPos == std::string::npos)
                throw std::runtime_error("Invalid group parameter format: " + arg);

            std::string name = arg.substr(0, eqPos);
            std::string value = arg.substr(eqPos + 1);

            const Definition* def = resolveGroupedKey(groupDefinition, name);
            if (!def)
                throw std::runtime_error("Unknown parameter '" + name + "' in group '" + groupName + "'");
            group[name] = parseValue(value, *def);

            ++index;
        }

        for (const auto& [key, def] : groupDefinition.keys) {
            if (key.ends_with(".[name]")) continue;
            if (group.contains(key)) continue;
            if (def.isRequired)
                throw std::runtime_error("Missing required parameter '" + key + "' in group '" + groupName + "'");
            if (!def.defaultValue.valueless_by_exception())
                group[key] = def.defaultValue;
        }

        groupInstances_[groupName].emplace_back(group, groupDefinition);
        return index;
    }

    void Manager::finalizeGlobalParameters() {
        for (const auto& [key, def] : definitions_) {
            if (values_.contains(key)) continue;

            if (def.isRequired && def.defaultValue.valueless_by_exception()) {
                std::string input;
                std::cout << key << " (required): ";
                std::getline(std::cin, input);
                values_[key] = parseValue(input, def);
            }
            else if (!def.isRequired && !def.defaultValue.valueless_by_exception()) {
                values_[key] = def.defaultValue;
            }
            else if (def.isRequired) {
                values_[key] = def.defaultValue;
            }
        }
    }

    std::string to_lowercase(const std::string& name) {
        std::string lower = name;
        transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower;
    }

    void Manager::showHelp() {
        constexpr int nameWidth = 30;

        std::cout << "Available options:\n";
        for (const auto& [key, def] : definitions_) {
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

        for (const auto& [group, def] : groupDefs_) {
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


    Value Manager::parseValue(const std::string& input, const Definition& def) {
        if (def.type == ValueType::Int) {
            try {
                return stoi(input);
            }
            catch (...) {
                throw std::runtime_error("Invalid integer: " + input);
            }
        }
        if (def.type == ValueType::Float) {
            try {
                return std::stof(input);
            }
            catch (...) {
                throw std::runtime_error("Invalid float: " + input);
            }
        }
        if (def.type == ValueType::Bool) {
            if (input == "true" || input == "1") {
                return true;
            }
            else if (input == "false" || input == "0") {
                return false;
            }
            else {
                throw std::runtime_error("Invalid boolean value: " + input);
            }
        }
        if (def.type == ValueType::PathExists) {
            if (!std::filesystem::exists(input)) {
                throw std::runtime_error("Path does not exist: " + input);
            }
        }

        return input;
    }
}
