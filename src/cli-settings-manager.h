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
#include <optional>
#include <iostream>
#include "app-error.h"

namespace CliSettings {

    enum class ValueType { String, Int, Float, Bool, PathExists };
    using Value = std::variant<std::string, int, bool, float>;
    using ValueMap = std::unordered_map<std::string, Value>;
    
    struct Definition {
        std::string description;
        bool isRequired;
        Value defaultValue;
        ValueType type;
    };
    struct GroupDefinition {
        std::string description;
		bool unique = false; 
        std::unordered_map<std::string, Definition> keys;
    };

    std::string to_lowercase(const std::string& name);

    /**
     * @brief Represents a single instance of a grouped CLI setting block (e.g., one --engine block).
     * Provides typed access to its key-value pairs using the same interface as top-level settings.
     */
    class GroupInstance {
    public:
        /**
         * @brief Constructs a group instance with reference to its values and definition.
         * @param values Map of key-value pairs parsed for this group instance.
         * @param keyDefs Map of expected keys and their metadata.
         */
        GroupInstance(const ValueMap& values, const GroupDefinition definition)
            : values_(values), definition_(definition) {
        }

        /**
         * @brief Retrieves the typed value of a group setting.
         * @tparam T Expected type: std::string, int, or bool.
         * @param name Key of the group setting.
         * @return Typed value of the parameter.
         * @throws std::runtime_error if the key is undefined or has invalid type.
         */
        template<typename T>
        T get(const std::string& name) const {
            std::string key = to_lowercase(name);
            auto it = values_.find(key);
			auto& keyDefs = definition_.keys;
            if (it == values_.end()) {
                auto defIt = keyDefs.find(key);
                if (defIt == keyDefs.end()) {
                    throw AppError::makeInvalidParameters("Access to undefined group setting: " + name);
                }
                return std::get<T>(defIt->second.defaultValue);
            }
            if (!std::holds_alternative<T>(it->second)) {
                if constexpr (std::is_same_v<T, int>) {
                    throw AppError::makeInvalidParameters("Expected integer for group setting \"" + name + "\".");
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    throw AppError::makeInvalidParameters("Expected boolean for group setting \"" + name + "\".");
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    throw AppError::makeInvalidParameters("Expected string for group setting \"" + name + "\".");
                }
                else if constexpr (std::is_same_v < T, float>) {
					throw AppError::makeInvalidParameters("Expected float for group setting \"" + name + "\".");
				}
            }
            return std::get<T>(it->second);
        }

        const GroupDefinition& getDefinition() const {
            return definition_;
        }

		const ValueMap& getValues() const {
			return values_;
		}

    private:
        const ValueMap values_;
        const GroupDefinition definition_;
    };

	using GroupInstances = std::vector<GroupInstance>;
    using GroupInstancesMap = std::unordered_map<std::string, GroupInstances>;

    /**
     * @brief Manages CLI parameters including types, validation, and interactive fallback.
     */
    class Manager {
    public:


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
		 * @param unique True if only one instance of this group is allowed.
         * @param keys Map of key names and their descriptions.
         */
        static void registerGroup(const std::string& groupName,
            const std::string& groupDescription,
            bool unique,
            const std::unordered_map<std::string, Definition>& keys);

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
            std::string key = to_lowercase(name);
            auto it = values_.find(key);
            if (it == values_.end()) {
                auto defIt = definitions_.find(key);
                if (defIt == definitions_.end()) {
                    throw std::runtime_error("Access to undefined setting: " + name);
                }
                return std::get<T>(defIt->second.defaultValue);
            }
            return std::get<T>(it->second);
        }

        /**
         * @brief Get a configuration group by name.
         * @param groupName Name of the group (e.g. "engine").
         * @return List of instances for this group
         * @throws std::runtime_error if the group is unknown.
         */
        static const GroupInstances getGroupInstances(const std::string& groupName);


        static const std::optional<GroupInstance> getGroupInstance(const std::string& groupName);

    private:

        struct ParsedParameter {
            std::string original;               // full raw input, for error reporting
            bool hasPrefix;                     // true if starts with "--"
            std::string name;                   // key part, never empty
            std::optional<std::string> value;   // optional value part
        };

        /**
         * Splits a raw command line argument into syntactic parts:
         * prefix (if any), name, and optional value.
         * Does not perform semantic interpretation.
         * @param raw The raw argument string, e.g. "--foo=bar" or "baz".
         * @return ParsedParameter with decomposed components.
         */
        static ParsedParameter parseParameter(const std::string& raw);

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
        static const Definition* resolveGroupedKey(const GroupDefinition& group, const std::string& name);

        /** Validates that the given default value matches the expected ValueType.
         *  Throws AppError if the type does not match or is semantically invalid.
         */
        static void validateDefaultValue(const std::string& name, const Value& value, ValueType type);

        /**
         * @brief Validates and finalizes all global parameters after parsing.
         * Throws if required values are missing.
         */
        static void finalizeGlobalParameters();

        static inline ValueMap values_;
        static inline std::unordered_map<std::string, Definition> definitions_;
        static inline std::unordered_map<std::string, GroupDefinition> groupDefs_;
        /**
         * @brief Stores grouped CLI values, organized by group name and ordered appearance.
         *        Each group name maps to a list of key-value maps (i.e., multiple blocks per group).
         */
        static inline GroupInstancesMap groupInstances_;

        static void showHelp();
        static Value parseValue(const ParsedParameter& arg, const Definition& def);
    };
}
