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
#include "../base-elements/file-helper.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <cassert>
#include <ranges>


namespace QaplaTester::Settings
{
    std::string value_to_string(const Value& value) {
        return std::visit([](auto&& val) {
            return std::format("{}", val);
        }, value);
    } 

    namespace
    {
        bool matchesCriteria(const GroupInstance& instance, const QaplaHelpers::IniFile::KeyValueMap& criteria) {
            using QaplaHelpers::to_lowercase;
            const auto& values = instance.getValues();

            return std::ranges::all_of(criteria, [&](const auto& entry) {
                const auto& [key, val] = entry; 
                const auto it = values.find(to_lowercase(key));
                if (it == values.end()) {
                    return false;
                }
                
                const std::string instanceValStr = value_to_string(it->second);
                return to_lowercase(instanceValStr) == to_lowercase(val);
            });
        }
    }

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
        return arg.value ? *arg.value : std::string();
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

    Value Manager::parsePathIsValid(const ParsedParameter& arg)
    {
        if (!arg.value)
        {
            throw AppError::makeInvalidParameters("Missing value for \"" + arg.original + "\"");
        }
        QaplaHelpers::validateOutputPath(*arg.value);
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
        case ValueType::ValidateOutputPath:
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
        definitions_[key] = {
            .description = config.description,
            .longDescription = config.longDescription,
            .isRequired = config.isRequired,
            .defaultValue = defaultValue,
            .type = config.type,
            .isHidden = config.isHidden
        };
    }

    void Manager::registerGroup(const GroupDefinition& config)
    {
        std::string key = QaplaHelpers::to_lowercase(config.name);
        QaplaHelpers::StableMap<std::string, ParameterDefinition> lowercaseKeys;
        for (const auto& [key, def] : config.keys) {
            const auto lowerKey = QaplaHelpers::to_lowercase(key);
            lowercaseKeys[lowerKey] = def;
        }

        groupDefs_[key] = config;
        groupDefs_[key].keys = std::move(lowercaseKeys);

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

    GroupInstances& Manager::getGroupInstancesMutable(const std::string &groupName)
    {
        std::string key = QaplaHelpers::to_lowercase(groupName);
        return groupInstances_[key];
    }

    void Manager::setGroupInstances(const std::string &groupName, const GroupInstances &instances)
    {
        std::string key = QaplaHelpers::to_lowercase(groupName);
        groupInstances_[key] = instances;
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

    void Manager::parseInput(const QaplaHelpers::ConfigData& configData, bool overwrite, bool strict)
    {
        handleHelpRequest(configData);
        
        // Process global parameters first
        for (const auto& [key, value] : configData.getGlobalParameters()) {
            parseGlobalParameter(key, value, overwrite);
        }
        
        processSections(configData, overwrite, strict);
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

    void Manager::processSections(const QaplaHelpers::ConfigData& configData, bool overwrite, bool strict)
    {
        for (const auto& sectionName : configData.getAllSectionNames()) {
            auto sectionMapOpt = configData.getSectionMap(sectionName);
            if (!sectionMapOpt.has_value()) {
                continue;
            }
            
            processSectionMap(*sectionMapOpt, overwrite, strict);
        }
    }

    void Manager::processSectionMap(const QaplaHelpers::ConfigData::SectionMap& sectionMap, bool overwrite, bool strict)
    {
        for (const auto& [sectionId, sectionList] : sectionMap) {
            if (!sectionList.empty()) {
                const auto groupName = QaplaHelpers::to_lowercase(sectionList[0].name);
                const auto defIt = groupDefs_.find(groupName);
                if (defIt != groupDefs_.end()) {
                    if (defIt->second.ignore) {
                        continue;
                    }
                    if (defIt->second.unique && sectionList.size() > 1) {
                        throw AppError::makeInvalidParameters("\"" + sectionList[0].name + "\" may only be specified once");
                    }
                }
            }
            
            for (const auto& section : sectionList) {
                parseGroupedParameter(section, overwrite, strict);
            }
        }
    }

    void Manager::parseGlobalParameter(const std::string& key, const std::string& value, bool overwrite)
    {
        std::string lowerKey = QaplaHelpers::to_lowercase(key);
        
        if (lowerKey == "help") {
            return; // Already handled in parseCommandLine
        }

        auto it = definitions_.find(lowerKey);
        if (it == definitions_.end()) {
            throw AppError::makeInvalidParameters("\"" + key + "\" is not a valid global parameter");
        }

        if (overwrite || !values_.contains(lowerKey)) {
            values_[lowerKey] = parseValue({.original = key + "=" + value, .hasPrefix = false, .name = lowerKey, .value = value}, it->second);
        }
    }

    ValueMap Manager::parseSectionEntries(const QaplaHelpers::IniFile::Section& section, 
                                           const GroupDefinition& groupDefinition,
                                           bool overwrite,
                                           const ValueMap* existingMap)
    {
        ValueMap result;
        if (existingMap != nullptr) {
            result = *existingMap;
        }
        
        for (const auto& [key, value] : section.entries) {
            const auto* def = resolveGroupedKey(groupDefinition, key);
            if (def == nullptr) {
                AppError::throwOnInvalidOption(groupDefinition.getKeyNames(), key,
                    std::format("Unknown parameter in section \"{}\"", section.name));
                throw AppError::makeInvalidParameters(
                    std::format("Unknown parameter \"{}\" in section \"{}\"", key, section.name));
            }
            const auto lowerKey = QaplaHelpers::to_lowercase(key);
            if (overwrite || !result.contains(lowerKey)) {
                result[lowerKey] = parseValue({
                    .original = std::format("{}={}", key, value), 
                    .hasPrefix = false, 
                    .name = lowerKey, 
                    .value = value}, *def);
            }
        }
        
        return result;
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

    bool Manager::matchesPrimaryKey(const QaplaHelpers::IniFile::Section& section,
                                    const GroupInstance& instance,
                                    const std::vector<std::string>& primaryKeys)
    {
        return std::ranges::all_of(primaryKeys, [&](const auto& keyName) {
            const auto sectionValue = section.getValue(keyName);
            if (!sectionValue.has_value() || !instance.isKeyProvided(keyName)) {
                return false;
            }

            const auto lowerKey = QaplaHelpers::to_lowercase(keyName);
            const auto instanceValue = instance.getValues().at(lowerKey);

            if (!std::holds_alternative<std::string>(instanceValue)) {
                return false;
            }

            return *sectionValue == std::get<std::string>(instanceValue);
        });
    }

    std::optional<size_t> Manager::findInstanceByPrimaryKey(GroupInstances& instances,
        const QaplaHelpers::IniFile::Section& section,
        const std::vector<std::string>& primaryKeys)
    {
        for (size_t i = 0; i < instances.size(); ++i) {
            if (matchesPrimaryKey(section, instances[i], primaryKeys)) {
                return i;
            }
        }
        return std::nullopt;
    }
    
    void Manager::parseGroupedParameter(const QaplaHelpers::IniFile::Section& section, bool overwrite, bool strict)
    {
        const auto groupName = QaplaHelpers::to_lowercase(section.name);
        
        const auto defIt = groupDefs_.find(groupName);
        if (defIt == groupDefs_.end()) {
            if (!strict) {
                return;
            }
            throw AppError::makeInvalidParameters("\"" + section.name + "\" is not a valid parameter group");
        }
        const auto& groupDefinition = defIt->second;

        if (groupDefinition.ignore) {
            return;
        }

        std::optional<size_t> matchingIndex;
        auto& instances = groupInstances_[groupName];

        if (groupDefinition.unique && instances.size() >= 1) {
            if (instances.size() > 1) {
                throw AppError::makeInvalidParameters("\"" + section.name + "\" may only be specified once");
            }
            matchingIndex = 0;
        } else if (groupInstances_.contains(groupName) && !groupDefinition.primaryKey.empty()) {
            matchingIndex = findInstanceByPrimaryKey(instances, section, groupDefinition.primaryKey);
        }
        
        if (matchingIndex.has_value()) {
            const auto mergedValues = parseSectionEntries(section, groupDefinition, overwrite, 
                &instances[*matchingIndex].getValues());
            validateExclusiveKeys(mergedValues, groupDefinition, section.name);
            instances[*matchingIndex] = GroupInstance(mergedValues, groupDefinition);
            return;
        }

        const auto newValues = parseSectionEntries(section, groupDefinition, false, nullptr);
        validateExclusiveKeys(newValues, groupDefinition, section.name);
        groupInstances_[groupName].emplace_back(newValues, groupDefinition);
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

    void Manager::validateCompleteness()
    {
        validateGroupCompleteness();
        validateGlobalParameterCompleteness();
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
        std::string lowerName = QaplaHelpers::to_lowercase(name);
        auto it = group.keys.find(lowerName);
        if (it != group.keys.end()) {
            return &it->second;
        }
        std::string postFix = "[name]";
        for (const auto &[key, def] : group.keys)
        {
            if (key.ends_with("." + postFix))
            {
                std::string prefix = key.substr(0, key.size() - postFix.length());
                if (lowerName.starts_with(prefix)) {
                    return &def;
                }
            }
        }

        return nullptr;
    }

    void Manager::validateGlobalParameterCompleteness()
    {
        for (const auto &[key, def] : definitions_)
        {
            if (values_.contains(key)) {
                continue;
            }

            if (def.isRequired && !def.defaultValue)
            {
                throw AppError::makeInvalidParameters(
                    std::format("Missing required global parameter \"{}\"", key));
            }
            if (def.defaultValue)
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
        case ValueType::ValidateOutputPath:
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
                    std::cout << std::format(" (default: {})", valueToString(*def.defaultValue));
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
                        std::cout << std::format(" (default: {})", valueToString(*meta.defaultValue));
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
            case ValueType::ValidateOutputPath:
                return parsePathIsValid(arg);
            default:
                return parseString(arg);
        }
    }

    QaplaHelpers::IniFile::SectionList Manager::groupInstancesToSectionList(
            const std::string& groupName, bool suppressDefault) const {
        QaplaHelpers::IniFile::SectionList result;
        
        const auto lowerGroupName = QaplaHelpers::to_lowercase(groupName);
        if (!groupInstances_.contains(lowerGroupName)) {
            return result;
        }
        
        const auto& instances = groupInstances_.at(lowerGroupName);
        result.reserve(instances.size());
        for (const auto& instance : instances) {
            result.push_back(instanceToSection(groupName, instance, suppressDefault));
        }
        
        return result;
    }

    QaplaHelpers::IniFile::Section Manager::instanceToSection(
            const std::string& groupName, const GroupInstance& instance, bool suppressDefault) {
        QaplaHelpers::IniFile::Section section;
        section.name = groupName;
        
        const auto& definition = instance.getDefinition();
        const auto& values = instance.getValues();
        
        for (const auto& keyName : definition.getKeyNames()) {
            if (!definition.keys.contains(keyName)) {
                continue;
            }
            const auto& keyDef = definition.keys.at(keyName);
            const auto hasValue = values.contains(keyName);
            
            const auto isRequired = keyDef.isRequired;
            const auto hasDefault = keyDef.defaultValue.has_value();
            const bool isDifferentFromDefault = hasValue && hasDefault && 
                    (values.at(keyName) != *keyDef.defaultValue);
            
            if (!suppressDefault || isRequired || !hasDefault || isDifferentFromDefault) {
                std::string keyToUse = keyName;
                constexpr std::string_view suffix = ".[name]";
                if (keyName.ends_with(suffix)) {
                    keyToUse = keyName.substr(0, keyName.size() - suffix.size());
                }
                
                std::string valueStr;
                if (hasValue) {
                    valueStr = valueToString(values.at(keyName));
                } else if (hasDefault) {
                    valueStr = valueToString(*keyDef.defaultValue);
                }
                
                if (!valueStr.empty()) {
                    section.addEntry(keyToUse, valueStr);
                }
            }
        }
        return section;
    }

    QaplaHelpers::IniFile::KeyValueMap Manager::getFilteredGlobalParameters() const {
        QaplaHelpers::IniFile::KeyValueMap result;
        
        for (const auto& [name, value] : values_) {
            const auto defIt = definitions_.find(name);
            if (defIt == definitions_.end()) {
                continue;
            }
            
            const auto& definition = defIt->second;
            const auto isRequired = definition.isRequired;
            const auto hasDefault = definition.defaultValue.has_value();
            const auto isDifferentFromDefault = hasDefault && (value != *definition.defaultValue);
            
            if (isRequired || !hasDefault || isDifferentFromDefault) {
                result.emplace_back(name, valueToString(value));
            }
        }
        
        return result;
    }

    QaplaHelpers::ConfigData Manager::toConfigData(
            const std::vector<std::string>& sectionNames,
            bool addGlobals) const {
        QaplaHelpers::ConfigData configData;
        
        std::vector<std::string> sectionsToExport = sectionNames;
        if (sectionsToExport.empty()) {
            for (const auto& [groupName, _] : groupInstances_) {
                sectionsToExport.push_back(groupName);
            }
        }
        
        for (const auto& sectionName : sectionsToExport) {
            auto sections = groupInstancesToSectionList(sectionName);
            for (auto& section : sections) {
                configData.addSection(section);
            }
        }
        
        if (addGlobals) {
            const auto globalParams = getFilteredGlobalParameters();
            for (const auto& [name, value] : globalParams) {
                configData.addGlobalParameter(name, value);
            }
        }
        
        return configData;
    }

    std::string Manager::valueToString(const Value& value)
    {
        return std::visit([](auto&& val) {
            return std::format("{}", val);
        }, value);
    }

    const GroupInstance* Manager::findGroupInstance(const std::string& groupName, 
                                                    const QaplaHelpers::IniFile::KeyValueMap& criteria) const
    {
        using QaplaHelpers::to_lowercase;
        auto lcGroup = to_lowercase(groupName);
        if (!groupInstances_.contains(lcGroup)) {
            return nullptr;
        }

        const auto& instances = groupInstances_.at(lcGroup);
        for (const auto& instance : instances) {
            if (matchesCriteria(instance, criteria)) {
                return &instance;
            }
        }
        return nullptr;
    }

    size_t Manager::removeGroupInstances(const std::string& groupName, 
                                         const QaplaHelpers::IniFile::KeyValueMap& criteria)
    {
        auto lcGroup = QaplaHelpers::to_lowercase(groupName);
        if (!groupInstances_.contains(lcGroup)) {
            return 0;
        }

        auto& instances = groupInstances_[lcGroup];
        auto oldSize = instances.size();
        
        std::erase_if(instances, [&](const GroupInstance& instance) {
            return matchesCriteria(instance, criteria);
        });
        
        return oldSize - instances.size();
    }

    QaplaHelpers::IniFile::Section Manager::groupInstanceToSection(const std::string& sectionName, 
                                                                   const GroupInstance& instance)
    {
        QaplaHelpers::IniFile::Section section;
        section.name = sectionName;
        
        for (const auto& [key, val] : instance.getValues()) {
            section.addEntry(key, valueToString(val));
        }
        
        return section;
    }

    void Manager::modifyGroupInstance(const QaplaHelpers::IniFile::Section& search,
                                      const QaplaHelpers::IniFile::Section& replace)
    {
         std::string groupName = search.name;
         
         const GroupInstance* instance = findGroupInstance(groupName, search.entries);
         if (instance == nullptr) {
             throw AppError::makeInvalidParameters("Could not find group instance to modify: " + groupName);
         }
         
         // Convert back to section
         QaplaHelpers::IniFile::Section newSection = groupInstanceToSection(groupName, *instance);
         
         // Apply edits
         for (const auto& [key, val] : replace.entries) {
             newSection.changeOrAddEntry(key, val);
         }
         
         // re-parse (update existing)
         parseGroupedParameter(newSection, true, true);
    }

} // namespace QaplaTester::CliSettings

