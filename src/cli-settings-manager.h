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
#pragma once

#include <string>
#include <variant>
#include <unordered_map>
#include <iostream>


 /**
  * @brief Manages CLI parameters including types, validation, and interactive fallback.
  */
class CliSettingsManager {
public:
    enum class ValueType { String, Int, Bool, PathExists };
    using Value = std::variant<std::string, int, bool>;
    using ValueMap = std::unordered_map<std::string, Value>;

    struct SettingDefinition {
        std::string description;
        bool isRequired;
        Value defaultValue;
        ValueType type;
    };

    /**
     * @brief Registers a setting with its metadata.
     * @param name Parameter name, case-insensitive.
     * @param description Help text for this parameter.
     * @param isRequired True if input is mandatory and must be provided.
     * @param defaultValue Default value if not required.
     * @param type Expected value type and validation mode.
     */
    static void registerSetting(const std::string& name,
        const std::string& description,
        bool isRequired,
        Value defaultValue,
        ValueType type = ValueType::String);

    /**
     * @brief Registers a grouped CLI block (e.g. --engine key=value...) with description and expected keys.
     * @param groupName Name of the group (e.g. \"engine\").
     * @param groupDescription Description shown in help output.
     * @param keys Map of key names and their descriptions.
     */
    static void registerGroup(const std::string& groupName,
        const std::string& groupDescription,
        const std::unordered_map<std::string, SettingDefinition>& keys);

    /**
     * @brief Parses CLI arguments in the format --name=value.
     * @param argc Argument count from main().
     * @param argv Argument vector from main().
     */
    static void parseCommandLine(int argc, char** argv);

    /**
     * @brief Retrieves the typed value of a setting.
     * @tparam T Expected type: std::string or int.
     * @param name Name of the parameter.
     * @return Typed value of the parameter.
     */
    template<typename T>
    static T get(const std::string& name) {
        std::string key = toLowercase(name);
        auto it = values.find(key);
        if (it == values.end()) {
			throw std::runtime_error("Access to undefined setting: " + name);
        }
        return std::get<T>(it->second);
    }

    /**
	 * @brief Get a configuration group by name.
     * @param groupName Name of the group (e.g. "engine").
     * @return Reference to the ValueMap group
     * @throws std::runtime_error if the group is unknown or empty.
     */
    static const std::vector<ValueMap>& getGroup(const std::string& groupName);

private:

    struct GroupDefinition {
        std::string description;
        std::unordered_map<std::string, SettingDefinition> keys;
    };

    /**
     * @brief Parses a single global parameter at the given position.
     * @param index Current index in argv array.
     * @param argc Total number of arguments.
     * @param argv Argument vector.
     * @return Index of the next unprocessed argument.
     */
    static int parseGlobalParameter(int index, int argc, char** argv);

    /**
     * @brief Parses an entire grouped parameter block starting at the given position.
     * @param index Current index pointing to the group name (--engine etc.).
     * @param argc Total number of arguments.
     * @param argv Argument vector.
     * @return Index of the next unprocessed argument after the group block.
     */
    static int parseGroupedParameter(int index, int argc, char** argv);

    /**
     * @brief Looks up a key definition in a group, supporting suffix wildcard match like option.X.
     * @param group The group definition to search.
     * @param name The key to resolve (e.g. option.Hash).
     * @return Pointer to matching definition, or nullptr if not found.
     */
    static const SettingDefinition* resolveGroupedKey(const GroupDefinition& group, const std::string& name);

    /**
     * @brief Validates and finalizes all global parameters after parsing.
     * Throws if required values are missing.
     */
    static void finalizeGlobalParameters();


	

    static inline std::unordered_map<std::string, SettingDefinition> definitions;
    static inline ValueMap values;
    static inline std::unordered_map<std::string, GroupDefinition> groupDefs;
    /**
     * @brief Stores grouped CLI values, organized by group name and ordered appearance.
     *        Each group name maps to a list of key-value maps (i.e., multiple blocks per group).
     */
    static inline std::unordered_map<std::string, std::vector<ValueMap>> groupedValues;

    static std::string toLowercase(const std::string& name);
    static void showHelp();
    static Value parseValue(const std::string& input, const SettingDefinition& def);
};
