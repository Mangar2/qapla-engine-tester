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

#include <string.h>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "app-error.h"
#include "string-helper.h"

namespace CliSettings
{

    std::vector<std::string> Manager::mergeWithSettingsFile(const std::vector<std::string> &originalArgs)
    {
        std::string filePath;
        for (size_t i = 1; i < originalArgs.size(); ++i)
        {
            const std::string &arg = originalArgs[i];
            if (arg.rfind("--settingsfile=", 0) == 0)
            {
                filePath = arg.substr(15);
                break;
            }
        }

        if (filePath.empty())
            return originalArgs;

        std::ifstream file(filePath);
        if (!file.is_open())
        {
            throw AppError::makeInvalidParameters("Failed to open settings file: " + filePath);
        }

        std::vector<std::string> settingsArgs = {originalArgs[0]}; // preserve program name
        std::vector<std::string> parsed = parseStreamToArgv(file);
        settingsArgs.insert(settingsArgs.end(), parsed.begin(), parsed.end());
        settingsArgs.insert(settingsArgs.end(), originalArgs.begin() + 1, originalArgs.end());
        return settingsArgs;
    }

    std::vector<std::string> Manager::parseStreamToArgv(std::istream &input)
    {
        std::vector<std::string> args;
        std::string line, section;
        int lineNumber = 0;

        while (std::getline(input, line))
        {
            ++lineNumber;
            line = trim(line);
            if (line.empty() || line[0] == '#')
                continue;

            if (auto maybeSection = parseSection(line))
            {
                section = *maybeSection;
                args.push_back("--" + section);
                continue;
            }

            auto kv = parseKeyValue(line);
            if (!kv)
            {
                throw AppError::makeInvalidParameters("Invalid setting in line " + std::to_string(lineNumber) +
                                                      ": '" + line + "'. Expected 'key=value' format.");
            }

            const auto &[key, value] = *kv;
            if (!section.empty())
            {
                args.push_back(key + "=" + value);
            }
            else
            {
                args.push_back("--" + key + "=" + value);
            }
        }

        return args;
    }

    void Manager::validateDefaultValue(const std::string &name, const Value &value, ValueType type)
    {
        auto typeMismatch = [&](const std::string &expected)
        {
            throw AppError::make("Default value for setting \"" + name + "\" must be of type " + expected + ".");
        };

        switch (type)
        {
        case ValueType::String:
            if (!std::holds_alternative<std::string>(value))
                typeMismatch("string");
            break;
        case ValueType::Int:
            if (!std::holds_alternative<int>(value))
                typeMismatch("int");
            break;
        case ValueType::Float:
            if (!std::holds_alternative<float>(value))
                typeMismatch("float");
            break;
        case ValueType::Bool:
            if (!std::holds_alternative<bool>(value))
                typeMismatch("bool");
            break;
        case ValueType::PathExists:
            if (!std::holds_alternative<std::string>(value) ||
                (!std::get<std::string>(value).empty() && std::get<std::string>(value) != "."))
            {
                typeMismatch("empty string required as default for type PathExists");
            }
            break;
        }
    }

    void Manager::registerSetting(const std::string &name,
                                  const std::string &description,
                                  bool isRequired,
                                  std::optional<Value> defaultValue,
                                  ValueType type)
    {

        if (defaultValue)
        {
            validateDefaultValue(name, *defaultValue, type);
        }

        std::string key = to_lowercase(name);
        definitions_[key] = {description, isRequired, defaultValue, type};
    }

    void Manager::registerGroup(const std::string &groupName,
                                const std::string &groupDescription,
                                bool unique,
                                const std::unordered_map<std::string, Definition> &keys)
    {
        for (const auto &[name, def] : keys)
        {
            if (!def.defaultValue)
                continue;
            validateDefaultValue(name, *def.defaultValue, def.type);
        }

        std::string key = to_lowercase(groupName);
        groupDefs_[key] = GroupDefinition{groupDescription, unique, keys};
    }

    const GroupInstances Manager::getGroupInstances(const std::string &groupName)
    {
        std::string key = to_lowercase(groupName);
        auto it = groupInstances_.find(key);
        if (it == groupInstances_.end() || it->second.empty())
        {
            return std::vector<GroupInstance>();
        }
        return it->second;
    }

    const std::optional<GroupInstance> Manager::getGroupInstance(const std::string &groupName)
    {
        std::string key = to_lowercase(groupName);
        auto it = groupInstances_.find(key);
        if (it == groupInstances_.end() || it->second.empty())
        {
            return std::nullopt;
        }
        if (it->second.size() == 0)
        {
            return std::nullopt;
        }
        return it->second[0];
    }

    Manager::ParsedParameter Manager::parseParameter(const std::string &raw)
    {
        ParsedParameter result;
        result.original = raw;

        std::string working = raw;

        result.hasPrefix = working.rfind("--", 0) == 0;
        if (result.hasPrefix)
        {
            working = working.substr(2);
        }

        auto eqPos = working.find('=');
        if (eqPos == std::string::npos)
        {
            result.name = to_lowercase(working);
            result.value = std::nullopt;
        }
        else
        {
            result.name = to_lowercase(working.substr(0, eqPos));
            result.value = working.substr(eqPos + 1);
        }

        return result;
    }

    void Manager::parseCommandLine(const std::vector<std::string> &args)
    {
        int index = 1;

        while (index < args.size())
        {
            auto arg = parseParameter(args[index]);

            if (arg.original == "--help")
            {
                showHelp();
                exit(0);
            }

            if (!arg.hasPrefix)
            {
                throw AppError::makeInvalidParameters("\"" + arg.original + "\" must start with \"--\"");
            }

            if (groupDefs_.find(arg.name) != groupDefs_.end())
            {
                index = parseGroupedParameter(index, args);
            }
            else
            {
                index = parseGlobalParameter(index, args);
            }
        }

        finalizeGlobalParameters();
    }

    int Manager::parseGlobalParameter(int index, const std::vector<std::string> &args)
    {
        auto arg = parseParameter(args[index]);

        if (!arg.hasPrefix)
            throw AppError::makeInvalidParameters("\"" + arg.original + "\" must be in the form --name=value");

        auto it = definitions_.find(arg.name);
        if (it == definitions_.end())
            throw AppError::makeInvalidParameters("\"" + arg.name + "\" is not a valid global parameter");

        values_[arg.name] = parseValue(arg, it->second);
        return index + 1;
    }

    SetResult Manager::setGlobalValue(const std::string &name, const std::string &value)
    {
        auto it = definitions_.find(name);
        if (it == definitions_.end())
        {
            return {SetResult::Status::UnknownName, "Unknown setting: \"" + name + "\""};
        }

        try
        {
            values_[name] = parseValue({.original = name + "=" + value, .hasPrefix = true, .name = name, .value = value}, it->second);
        }
        catch (const AppError &ex)
        {
            return {SetResult::Status::InvalidValue, ex.what()};
        }

        return {SetResult::Status::Success, {}};
    }

    const Definition *Manager::resolveGroupedKey(const GroupDefinition &group, const std::string &name)
    {
        auto it = group.keys.find(name);
        if (it != group.keys.end())
            return &it->second;
        std::string postFix = "[name]";
        for (const auto &[key, def] : group.keys)
        {
            if (key.ends_with("." + postFix))
            {
                std::string prefix = key.substr(0, key.size() - postFix.length());
                if (name.rfind(prefix, 0) == 0)
                    return &def;
            }
        }

        return nullptr;
    }

    int Manager::parseGroupedParameter(int index, const std::vector<std::string> &args)
    {
        auto groupArg = parseParameter(args[index]);
        index++;

        auto defIt = groupDefs_.find(groupArg.name);
        if (defIt == groupDefs_.end())
            throw AppError::makeInvalidParameters("\"" + groupArg.name + "\" is not a valid parameter");

        const auto &groupDefinition = defIt->second;
        ValueMap group;

        if (groupDefinition.unique && groupInstances_.contains(groupArg.name))
        {
            throw AppError::makeInvalidParameters("\"" + groupArg.name + "\" may only be specified once");
        }

        while (index < args.size())
        {
            auto arg = parseParameter(args[index]);

            // this is not a parameter of the group, so we stop parsing
            if (arg.hasPrefix)
                break;

            const Definition *def = resolveGroupedKey(groupDefinition, arg.name);
            if (!def)
                throw AppError::makeInvalidParameters(
                    "Unknown parameter \"" + arg.name + "\" in section \"" + groupArg.name + "\"");
            group[arg.name] = parseValue(arg, *def);

            ++index;
        }

        for (const auto &[key, def] : groupDefinition.keys)
        {
            if (key.ends_with(".[name]"))
                continue;
            if (group.contains(key))
                continue;
            if (def.isRequired)
                throw AppError::makeInvalidParameters(
                    "Missing required parameter \"" + key + "\" in section \"" + groupArg.name + "\"");
            if (def.defaultValue)
                group[key] = *def.defaultValue;
        }

        groupInstances_[groupArg.name].emplace_back(group, groupDefinition);
        return index;
    }

    void Manager::finalizeGlobalParameters()
    {
        for (const auto &[key, def] : definitions_)
        {
            if (values_.contains(key))
                continue;

            if (def.isRequired && !def.defaultValue)
            {
                std::string input;
                std::cout << key << " (required): ";
                std::getline(std::cin, input);
                values_[key] = parseValue(
                    ParsedParameter{.hasPrefix = false, .name = key, .value = input}, def);
            }
            else if (!def.isRequired && def.defaultValue)
            {
                values_[key] = *def.defaultValue;
            }
            else if (def.isRequired)
            {
                values_[key] = *def.defaultValue;
            }
        }
    }

    void Manager::showHelp()
    {
        constexpr int nameWidth = 30;

        std::cout << "Available options:\n";
        for (const auto &[key, def] : definitions_)
        {
            std::ostringstream line;
            line << "  --" << key << "=";

            std::string typeStr = (def.type == ValueType::Int) ? "<int>" : (def.type == ValueType::Bool)     ? "<bool>"
                                                                       : (def.type == ValueType::PathExists) ? "<path>"
                                                                                                             : "<string>";

            line << typeStr;
            std::cout << std::left << std::setw(nameWidth) << line.str();

            std::cout << def.description;
            if (def.isRequired)
                std::cout << " [required]";
            else if (def.defaultValue)
            {
                bool isEmptyString = std::holds_alternative<std::string>(*def.defaultValue) && std::get<std::string>(*def.defaultValue).empty();
                if (!isEmptyString)
                {
                    std::cout << " (default: ";
                    std::visit([](auto &&v)
                               { std::cout << v; }, *def.defaultValue);
                    std::cout << ")";
                }
            }
            std::cout << "\n";
        }

        for (const auto &[group, def] : groupDefs_)
        {
            std::ostringstream header;
            header << "  --" << group << " ...";

            std::cout << "\n"
                      << std::left << std::setw(nameWidth) << header.str();
            std::cout << def.description << "\n";

            for (const auto &[param, meta] : def.keys)
            {
                std::ostringstream line;
                line << "    " << param << "=";

                std::string typeStr = (meta.type == ValueType::Int) ? "<int>" : (meta.type == ValueType::Bool)     ? "<bool>"
                                                                            : (meta.type == ValueType::PathExists) ? "<path>"
                                                                                                                   : "<string>";

                line << typeStr;
                std::cout << std::left << std::setw(nameWidth) << line.str();

                std::cout << meta.description;
                if (meta.isRequired)
                    std::cout << " [required]";
                else if (meta.defaultValue)
                {
                    bool isEmptyString = std::holds_alternative<std::string>(*meta.defaultValue) && std::get<std::string>(*meta.defaultValue).empty();
                    if (!isEmptyString)
                    {
                        std::cout << " (default: ";
                        std::visit([](auto &&v)
                                   { std::cout << v; }, *meta.defaultValue);
                        std::cout << ")";
                    }
                }
                std::cout << "\n";
            }
        }
    }

    Value Manager::parseValue(const ParsedParameter &arg, const Definition &def)
    {
        auto lowerValue = arg.value ? to_lowercase(*arg.value) : std::string();
        if (def.type == ValueType::Bool)
        {
            if (!arg.value)
                return true;
            if (lowerValue == "true" || lowerValue == "1")
            {
                return true;
            }
            else if (lowerValue == "false" || lowerValue == "0")
            {
                return false;
            }
            else
            {
                throw AppError::makeInvalidParameters("\"" + arg.original + "\" is invalid: expected true, false, 1 or 0");
            }
        }
        if (!arg.value)
        {
            throw AppError::makeInvalidParameters("Missing value for \"" + arg.original + "\"");
        }
        if (def.type == ValueType::Int)
        {
            try
            {
                return stoi(*arg.value);
            }
            catch (...)
            {
                throw AppError::makeInvalidParameters("\"" + arg.original + "\" is invalid: expected integer");
            }
        }
        if (def.type == ValueType::Float)
        {
            try
            {
                return std::stof(*arg.value);
            }
            catch (...)
            {
                throw AppError::makeInvalidParameters("\"" + arg.original + "\" is invalid: expected float");
            }
        }
        if (def.type == ValueType::PathExists)
        {
            if (!std::filesystem::exists(*arg.value))
            {
                throw AppError::makeInvalidParameters("The path in \"" + arg.original + "\" does not exist");
            }
            return *arg.value;
        }
        if (def.type == ValueType::PathParentExists)
        {
            std::filesystem::path path(*arg.value);
            std::filesystem::path parent = path.parent_path();
            if (!std::filesystem::exists(parent))
            {
                throw AppError::makeInvalidParameters("The parent directory of \"" + arg.original + "\" does not exist");
            }
            return *arg.value;
        }

        return lowerValue; // Default case is string;
    }
}
