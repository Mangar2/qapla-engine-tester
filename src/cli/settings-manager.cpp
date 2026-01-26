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

#include "settings-manager.h"

#include "../base-elements/app-error.h"
#include "../base-elements/string-helper.h"
#include "../base-elements/ini-file.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <cassert>


namespace QaplaTester::Settings
{

    Value Manager::parseBool(const ParsedParameter& arg)
    {
        auto lowerValue = arg.value ? QaplaHelpers::to_lowercase(*arg.value) : std::string();
        if (!arg.value) {
            return true;
        }
        if (lowerValue == "true" || lowerValue == "1")
        {
            return true;
        }
        if (lowerValue == "false" || lowerValue == "0")
        {
            return false;
        }
        throw AppError::makeInvalidParameters("\"" + arg.original + "\" is invalid: expected true, false, 1 or 0");
    }

    Value Manager::parseInt(const ParsedParameter& arg)
    {
        if (!arg.value)
        {
            throw AppError::makeInvalidParameters("Missing value for \"" + arg.original + "\"");
        }
        auto result = QaplaHelpers::to_int(*arg.value);
        if (!result)
        {
            throw AppError::makeInvalidParameters("\"" + arg.original + "\" is invalid: expected integer");
        }
        return *result;
    }

    Value Manager::parseUInt(const ParsedParameter& arg)
    {
        if (!arg.value)
        {
            throw AppError::makeInvalidParameters("Missing value for \"" + arg.original + "\"");
        }
        auto result = QaplaHelpers::to_uint32(*arg.value);
        if (!result)
        {
            throw AppError::makeInvalidParameters("\"" + arg.original + "\" is invalid: expected positive integer");
        }
        return *result;
    }

    Value Manager::parseFloat(const ParsedParameter& arg)
    {
        if (!arg.value)
        {
            throw AppError::makeInvalidParameters("Missing value for \"" + arg.original + "\"");
        }
        auto result = QaplaHelpers::to_double(*arg.value);
        if (!result)
        {
            throw AppError::makeInvalidParameters("\"" + arg.original + "\" is invalid: expected double");
        }
        return *result;
    }

    Value Manager::parseString(const ParsedParameter& arg)
    {
        return arg.value ? QaplaHelpers::to_lowercase(*arg.value) : std::string();
    }

    Value Manager::parsePathExists(const ParsedParameter& arg)
    {
        if (!arg.value)
        {
            throw AppError::makeInvalidParameters("Missing value for \"" + arg.original + "\"");
        }
        if (!std::filesystem::exists(*arg.value))
        {
            throw AppError::makeInvalidParameters("The path in \"" + arg.original + "\" does not exist");
        }
        return *arg.value;
    }

    Value Manager::parsePathParentExists(const ParsedParameter& arg)
    {
        if (!arg.value)
        {
            throw AppError::makeInvalidParameters("Missing value for \"" + arg.original + "\"");
        }
        std::filesystem::path path(*arg.value);
        std::filesystem::path parent = path.parent_path();
        if (parent.empty()) {
            parent = std::filesystem::current_path();
        }
        if (!std::filesystem::exists(parent))
        {
            throw AppError::makeInvalidParameters("The parent directory of \"" + arg.original + "\" does not exist");
        }
        return *arg.value;
    }

    void Manager::validateDefaultValue(const std::string &name, const Value &value, ValueType type)
    {
        auto typeMismatch = [&](const std::string &expected)
        {
            throw AppError::makeInvalidParameters(std::format("Default value for setting \"{}\" must be of type {}.", name, expected));
        };

        switch (type)
        {
        case ValueType::Int:
            if (!std::holds_alternative<int>(value)) {
                typeMismatch("int");
            }
            break;
        case ValueType::UInt:
            if (!std::holds_alternative<unsigned int>(value)) {
                typeMismatch("unsigned int");
            }
            break;
        case ValueType::Float:
            if (!std::holds_alternative<double>(value)) {
                typeMismatch("double");
            }
            break;
        case ValueType::Bool:
            if (!std::holds_alternative<bool>(value)) {
                typeMismatch("bool");
            }
            break;
        case ValueType::PathExists:
            if (!std::holds_alternative<std::string>(value) ||
                (!std::get<std::string>(value).empty() && std::get<std::string>(value) != "."))
            {
                typeMismatch("empty string required as default for type PathExists");
            }
            break;
        case ValueType::PathParentExists:
            if (!std::holds_alternative<std::string>(value) ||
                (!std::get<std::string>(value).empty() && std::get<std::string>(value) != "."))
            {
                typeMismatch("empty string required as default for type PathParentExists");
            }
            break;
        default:
            if (!std::holds_alternative<std::string>(value)) {
                typeMismatch("string");
            }
            break;
        }
    }

    Manager::ParsedParameter Manager::parseParameter(const std::string &raw)
    {
        ParsedParameter result;
        result.original = raw;

        std::string working = raw;

        result.hasPrefix = working.starts_with("--");
        if (result.hasPrefix)
        {
            working = working.substr(2);
        }

        auto eqPos = working.find('=');
        if (eqPos == std::string::npos)
        {
            result.name = QaplaHelpers::to_lowercase(working);
            result.value = std::nullopt;
        }
        else
        {
            result.name = QaplaHelpers::to_lowercase(working.substr(0, eqPos));
            result.value = working.substr(eqPos + 1);
        }

        return result;
    }

    void Manager::registerSetting(const GlobalDefinition& config)
    {
        auto defaultValue = config.defaultValue;

        if (defaultValue)
        {
            if (config.type == ValueType::UInt && std::holds_alternative<int>(*defaultValue) 
                && (std::get<int>(*defaultValue) >= 0))
            {
                defaultValue = static_cast<unsigned int>(std::get<int>(*defaultValue));
			}
            validateDefaultValue(config.name, *defaultValue, config.type);
        }

        std::string key = QaplaHelpers::to_lowercase(config.name);
        definitions_[key] = {.description = config.description, .isRequired = config.isRequired, .defaultValue = defaultValue, .type = config.type};
    }

    void Manager::registerGroup(const GroupDefinition& config)
    {
        std::string key = QaplaHelpers::to_lowercase(config.name);
        groupDefs_[key] = config;

        for (auto& [name, def] : groupDefs_[key].keys)
        {
            if (!def.defaultValue) {
                continue;
            }
			auto type = def.type;
			auto& defaultValue = def.defaultValue;
            if (type == ValueType::UInt && std::holds_alternative<int>(*defaultValue)
                && (std::get<int>(*defaultValue) >= 0))
            {
                *defaultValue = static_cast<unsigned int>(std::get<int>(*defaultValue));
            }
            validateDefaultValue(name, *def.defaultValue, def.type);
        }
    }

    GroupInstances Manager::getGroupInstances(const std::string &groupName)
    {
        std::string key = QaplaHelpers::to_lowercase(groupName);
        auto it = groupInstances_.find(key);
        if (it == groupInstances_.end() || it->second.empty())
        {
            return {};
        }
        return it->second;
    }

    std::optional<GroupInstance> Manager::getGroupInstance(const std::string &groupName)
    {
        std::string key = QaplaHelpers::to_lowercase(groupName);
        auto it = groupInstances_.find(key);
        if (it == groupInstances_.end() || it->second.empty())
        {
            return std::nullopt;
        }
        if (it->second.empty())
        {
            return std::nullopt;
        }
        return it->second[0];
    }

    void Manager::parseInput(const QaplaHelpers::ConfigData& configData, bool strict)
    {
        handleHelpRequest(configData);
        
        // Process global parameters first
        for (const auto& [key, value] : configData.getGlobalParameters()) {
            parseGlobalParameter(key, value, strict);
        }
        
        processSections(configData, strict);
        finalizeGlobalParameters();
    }

    void Manager::handleHelpRequest(const QaplaHelpers::ConfigData& configData)
    {
        for (const auto& [key, value] : configData.getGlobalParameters()) {
            if (QaplaHelpers::to_lowercase(key) == "help") {
                showHelp();
                exit(0);
            }
        }
    }

    void Manager::processSections(const QaplaHelpers::ConfigData& configData, bool strict)
    {
        for (const auto& sectionName : configData.getAllSectionNames()) {
            auto sectionMapOpt = configData.getSectionMap(sectionName);
            if (!sectionMapOpt) {
                continue;
            }
            
            processSectionMap(*sectionMapOpt, strict);
        }
    }

    void Manager::processSectionMap(const QaplaHelpers::ConfigData::SectionMap& sectionMap, bool strict)
    {
        for (const auto& [sectionId, sectionList] : sectionMap) {
            for (const auto& section : sectionList) {
                processSection(section, strict);
            }
        }
    }

    void Manager::processSection(const QaplaHelpers::IniFile::Section& section, bool strict)
    {
        if (groupDefs_.contains(QaplaHelpers::to_lowercase(section.name))) {
            parseGroupedParameter(section, strict);
        } else {
            processSectionEntries(section, strict);
        }
    }

    void Manager::processSectionEntries(const QaplaHelpers::IniFile::Section& section, bool strict)
    {
        for (const auto& [key, value] : section.entries) {
            parseGlobalParameter(key, value, strict);
        }
    }

    void Manager::parseGlobalParameter(const std::string& key, const std::string& value, bool strict)
    {
        std::string lowerKey = QaplaHelpers::to_lowercase(key);
        
        if (lowerKey == "help") {
            return; // Already handled in parseCommandLine
        }

        auto it = definitions_.find(lowerKey);
        if (it == definitions_.end()) {
            if (!strict) {
                return;
            }
            throw AppError::makeInvalidParameters("\"" + key + "\" is not a valid global parameter");
        }

        values_[lowerKey] = parseValue({.original = key + "=" + value, .hasPrefix = false, .name = lowerKey, .value = value}, it->second);
    }

    ValueMap Manager::parseSectionEntries(const QaplaHelpers::IniFile::Section& section, 
                                           const GroupDefinition& groupDefinition, 
                                           bool strict)
    {
        ValueMap group;
        
        for (const auto& [key, value] : section.entries) {
            std::string lowerKey = QaplaHelpers::to_lowercase(key);
            
            const ParameterDefinition *def = resolveGroupedKey(groupDefinition, lowerKey);
            if (def == nullptr) {
                if (!strict) {
                    continue;
                }
                AppError::throwOnInvalidOption(groupDefinition.keyNames(), key,
                    std::format("Unknown parameter in section \"{}\"", section.name));
            }
            group[lowerKey] = parseValue({
                .original = std::format("{}={}", key, value), 
                .hasPrefix = false, 
                .name = lowerKey, 
                .value = value}, *def);
        }
        
        return group;
    }

    void Manager::validateExclusiveKeys(const ValueMap& group, 
                                        const GroupDefinition& groupDefinition, 
                                        const std::string& sectionName)
    {
        for (const auto &[key, def] : groupDefinition.keys) {
            if (def.exclusive && group.contains(key) && group.size() > 1) {
                throw AppError::makeInvalidParameters(
                    std::format(
                        R"(Parameter "{}" in section "{}" cannot be combined with other parameters)", 
                        key, sectionName));
            }
        }
    }

    void Manager::parseGroupedParameter(const QaplaHelpers::IniFile::Section& section, bool strict)
    {
        std::string groupName = QaplaHelpers::to_lowercase(section.name);
        
        auto defIt = groupDefs_.find(groupName);
        if (defIt == groupDefs_.end()) {
            throw AppError::makeInvalidParameters("\"" + section.name + "\" is not a valid parameter group");
        }

        const auto &groupDefinition = defIt->second;

        if (groupDefinition.unique && groupInstances_.contains(groupName))
        {
            throw AppError::makeInvalidParameters("\"" + section.name + "\" may only be specified once");
        }

        ValueMap group = parseSectionEntries(section, groupDefinition, strict);
        validateExclusiveKeys(group, groupDefinition, section.name);
        groupInstances_[groupName].emplace_back(group, groupDefinition);
    }

    void Manager::mergeSectionList(const std::string& sectionName,
                                   const QaplaHelpers::IniFile::SectionList& sections,
                                   const std::string& mergeIdentifier,
                                   bool strict)
    {
        std::string groupName = QaplaHelpers::to_lowercase(sectionName);
        
        auto defIt = groupDefs_.find(groupName);
        if (defIt == groupDefs_.end()) {
            throw AppError::makeInvalidParameters("\"" + sectionName + "\" is not a valid parameter group");
        }

        const auto& groupDefinition = defIt->second;

        for (const auto& section : sections) {
            ValueMap group = parseSectionEntries(section, groupDefinition, strict);
            validateExclusiveKeys(group, groupDefinition, section.name);

            GroupInstance newInstance(group, groupDefinition);
            mergeOrAppendInstance(groupName, newInstance, groupDefinition, section, mergeIdentifier);
        }
    }

    void Manager::mergeConfigData(const QaplaHelpers::ConfigData& configData, bool strict)
    {
        // Merge global parameters
        for (const auto& [key, value] : configData.getGlobalParameters()) {
            parseGlobalParameter(key, value, strict);
        }
        
        // Merge grouped sections
        auto sectionNames = configData.getAllSectionNames();
        for (const auto& sectionName : sectionNames) {
            std::string groupName = QaplaHelpers::to_lowercase(sectionName);
            
            auto defIt = groupDefs_.find(groupName);
            if (defIt == groupDefs_.end()) {
                if (strict) {
                    throw AppError::makeInvalidParameters("\"" + sectionName + "\" is not a valid parameter group");
                }
                continue;
            }

            auto sectionMap = configData.getSectionMap(sectionName);
            if (!sectionMap) {
                continue;
            }

            for (const auto& [sectionId, sectionList] : *sectionMap) {
                std::string mergeIdentifier = (groupName == "engine") ? "name" : "";
                mergeSectionList(sectionName, sectionList, mergeIdentifier, strict);
            }
        }
    }

    void Manager::mergeOrAppendInstance(const std::string& groupName,
                                        const GroupInstance& newInstance,
                                        const GroupDefinition& groupDefinition,
                                        const QaplaHelpers::IniFile::Section& section,
                                        const std::string& mergeIdentifier)
    {
        if (groupDefinition.unique && groupInstances_.contains(groupName)) {
            auto& instances = groupInstances_[groupName];
            GroupInstance merged = newInstance.merge(instances[0]);
            instances = GroupInstances{};
            instances.emplace_back(std::move(merged));
        }
        else if (!groupDefinition.unique && !mergeIdentifier.empty() && groupInstances_.contains(groupName)) {
            auto& instances = groupInstances_[groupName];
            auto newIdValue = section.getValue(mergeIdentifier);
            
            if (newIdValue && tryMergeByIdentifier(instances, newInstance, mergeIdentifier, *newIdValue)) {
                return;
            }
            instances.emplace_back(newInstance);
        }
        else {
            groupInstances_[groupName].emplace_back(newInstance);
        }
    }

    bool Manager::tryMergeByIdentifier(GroupInstances& instances,
                                       const GroupInstance& newInstance,
                                       const std::string& mergeIdentifier,
                                       const std::string& newIdValue)
    {
        GroupInstances newInstances;
        bool merged = false;
        
        for (const auto& existingInstance : instances) {
            if (!merged && existingInstance.isKeyProvided(mergeIdentifier)) {
                auto existingIdValue = existingInstance.get<std::string>(mergeIdentifier);
                if (existingIdValue == newIdValue) {
                    newInstances.emplace_back(newInstance.merge(existingInstance));
                    merged = true;
                    continue;
                }
            }
            newInstances.emplace_back(existingInstance);
        }
        
        if (merged) {
            instances = std::move(newInstances);
        }
        return merged;
    }

    void Manager::validateGroupCompleteness()
    {
        // Validate all group instances for required parameters and add defaults
        for (auto& [groupName, instances] : groupInstances_) {
            auto defIt = groupDefs_.find(groupName);
            if (defIt == groupDefs_.end()) {
                continue; // Should not happen, but skip if definition not found
            }

            const auto& groupDefinition = defIt->second;

            // We need to rebuild the instances vector with complete instances
            GroupInstances completeInstances;
            completeInstances.reserve(instances.size());

            for (const auto& instance : instances) {
                ValueMap completeGroup = instance.getValues();

                // Check for required parameters and add defaults
                for (const auto& [key, def] : groupDefinition.keys) {
                    if (key.ends_with(".[name]")) {
                        continue;
                    }
                    if (completeGroup.contains(key)) {
                        continue;
                    }
                    if (def.isRequired) {
                        throw AppError::makeInvalidParameters(
                            std::format(R"(Missing required parameter "{}" in group "{}")", 
                                key, groupName));
                    }
                    if (def.defaultValue) {
                        completeGroup[key] = *def.defaultValue;
                    }
                }

                completeInstances.emplace_back(completeGroup, groupDefinition);
            }

            instances = std::move(completeInstances);
        }
    }

    SetResult Manager::setGlobalValue(const std::string &name, const std::string &value)
    {
        auto it = definitions_.find(name);
        if (it == definitions_.end())
        {
            return {.status = SetResult::Status::UnknownName, .errorMessage = "Unknown setting: \"" + name + "\""};
        }

        try
        {
            values_[name] = parseValue({.original = name + "=" + value, .hasPrefix = true, .name = name, .value = value}, it->second);
        }
        catch (const AppError &ex)
        {
            return {.status = SetResult::Status::InvalidValue, .errorMessage = ex.what()};
        }

        return {.status = SetResult::Status::Success, .errorMessage = {}};
    }

    const ParameterDefinition *Manager::resolveGroupedKey(const GroupDefinition &group, const std::string &name)
    {
        auto it = group.keys.find(name);
        if (it != group.keys.end()) {
            return &it->second;
        }
        std::string postFix = "[name]";
        for (const auto &[key, def] : group.keys)
        {
            if (key.ends_with("." + postFix))
            {
                std::string prefix = key.substr(0, key.size() - postFix.length());
                if (name.starts_with(prefix)) {
                    return &def;
                }
            }
        }

        return nullptr;
    }

    void Manager::finalizeGlobalParameters()
    {
        for (const auto &[key, def] : definitions_)
        {
            if (values_.contains(key)) {
                continue;
            }

            if (def.isRequired && !def.defaultValue)
            {
                std::string input;
                std::cout << key << " (required): ";
                std::getline(std::cin, input);
                values_[key] = parseValue(
                    ParsedParameter{.original="", .hasPrefix = false, .name = key, .value = input}, def);
            }
            else if (def.defaultValue)
            {
                values_[key] = *def.defaultValue;
            }
        }
    }

    static std::string to_string(ValueType type) {
        switch (type)
        {
        case ValueType::Int:
        case ValueType::UInt:
        case ValueType::Float:
            return "<number>";
        case ValueType::Bool:
            return "<bool>";
        case ValueType::PathExists:
        case ValueType::PathParentExists:
            return "<path>";
        default:
            return "string";
		}

    }

    void Manager::showHelp()
    {
        constexpr int nameWidth = 30;

        std::cout << "Available options:\n";
        for (const auto &[key, def] : definitions_)
        {
            if (def.isHidden) {
                continue;
            }

            std::ostringstream line;
            line << "  --" << key << "=";

			std::string typeStr = to_string(def.type);

            line << typeStr;
            std::cout << std::left << std::setw(nameWidth) << line.str();

            std::cout << def.description;
            if (def.isRequired) {
                std::cout << " [required]";
            }
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
                if (meta.isHidden) {
                    continue;
                }

                std::ostringstream line;
                line << "    " << param << "=";

				std::string typeStr = to_string(meta.type);

                line << typeStr;
                std::cout << std::left << std::setw(nameWidth) << line.str();

                std::cout << meta.description;
                if (meta.isRequired) {
                    std::cout << " [required]";
                }
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

    Value Manager::parseValue(const ParsedParameter &arg, const ParameterDefinition &def)
    {
        switch (def.type) {
            case ValueType::Bool:
                return parseBool(arg);
            case ValueType::Int:
                return parseInt(arg);
            case ValueType::UInt:
                return parseUInt(arg);
            case ValueType::Float:
                return parseFloat(arg);
            case ValueType::PathExists:
                return parsePathExists(arg);
            case ValueType::PathParentExists:
                return parsePathParentExists(arg);
            default:
                return parseString(arg);
        }
    }
} // namespace QaplaTester::CliSettings
