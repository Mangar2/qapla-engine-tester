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
#include <vector>
#include "app-error.h"
#include "string-helper.h"

namespace QaplaTester::CliSettings {

    enum class ValueType : std::uint8_t { String, Int, UInt, Float, Bool, PathExists, PathParentExists };
    using Value = std::variant<std::string, int, unsigned int, bool, double>;
    using ValueMap = std::unordered_map<std::string, Value>;
    
    struct Definition {
        std::string description;
        bool isRequired;
        std::optional<Value> defaultValue;
        ValueType type;
    };
    struct GroupDefinition {
        std::string description;
		bool unique = false; 
        std::unordered_map<std::string, Definition> keys;

        /**
         * Returns all defined keys in the group. Keys ending with ".[name]" are returned without that suffix.
        */
        [[nodiscard]] std::vector<std::string> keyNames() const {
            std::vector<std::string> result;
            result.reserve(keys.size());
            constexpr std::string_view suffix = ".[name]";
            for (const auto& [key, _] : keys) {
                if (key.ends_with(suffix)) {
                    result.emplace_back(key.substr(0, key.size() - suffix.size()));
                } else {
                    result.emplace_back(key);
                }
            }
            return result;
        }
    };

    /**
     * @brief Represents a single instance of a grouped CLI setting block (e.g., one --engine block).
     * Provides typed access to its key-value pairs using the same interface as top-level settings.
     */
    class GroupInstance {
    public:
        /**
         * @brief Constructs a group instance with reference to its values and definition.
         * @param values Map of key-value pairs parsed for this group instance.
		 * @param definition Definition of the group, including expected keys and their metadata.
         */
        GroupInstance(ValueMap values, GroupDefinition definition)
            : values_(std::move(values)), definition_(std::move(definition)) {
        }

        /**
         * @brief Retrieves the typed value of a group setting.
         * @tparam T Expected type: std::string, int, or bool.
         * @param name Key of the group setting.
         * @return Typed value of the parameter.
         * @throws std::runtime_error if the key is undefined or has invalid type.
         */
        template<typename T>
        [[nodiscard]] T get(const std::string& name) const {
            std::string key = QaplaHelpers::to_lowercase(name);
            auto it = values_.find(key);
			const auto& keyDefs = definition_.keys;
            if (it == values_.end()) {
                auto defIt = keyDefs.find(key);
                if (defIt == keyDefs.end() || !defIt->second.defaultValue) {
                    throw AppError::makeInvalidParameters("Access to undefined group setting: " + name);
                }
                return std::get<T>(*defIt->second.defaultValue);
            }
            if (!std::holds_alternative<T>(it->second)) {
                if constexpr (std::is_same_v<T, int>) {
                    throw AppError::makeInvalidParameters("Expected whole number for group setting \"" + name + "\".");
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    throw AppError::makeInvalidParameters("Expected true or false for group setting \"" + name + "\".");
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    throw AppError::makeInvalidParameters("Expected string for group setting \"" + name + "\".");
                }
                else if constexpr (std::is_same_v < T, double>) {
					throw AppError::makeInvalidParameters("Expected decimal number for group setting \"" + name + "\".");
				}
                else if constexpr (std::is_same_v<T, unsigned int>) {
                    throw AppError::makeInvalidParameters("Expected positive whole number for group setting \"" + name + "\".");
                }
            }
            auto value = std::get<T>(it->second);
            return value;
        }

        [[nodiscard]] const GroupDefinition& getDefinition() const {
            return definition_;
        }

		[[nodiscard]] const ValueMap& getValues() const {
			return values_;
		}

    private:
        const ValueMap values_;
        const GroupDefinition definition_;
    };

	using GroupInstances = std::vector<GroupInstance>;
    using GroupInstancesMap = std::unordered_map<std::string, GroupInstances>;

    struct SetResult {
        enum class Status : std::uint8_t {
            Success,
            UnknownName,
            InvalidValue
        };

        Status status;
        std::string errorMessage; // empty if success
    };

    /**
     * @brief Manages CLI parameters including types, validation, and interactive fallback.
     */
    class Manager {
    public:

        /**
         * Merges the original command line arguments with settings from a file.
         * If a settings file is specified, it will be parsed and its contents
         * will be added to the original arguments.
         * @param originalArgs Original command line arguments.
         * @return Merged vector of command line arguments.
         */
        static std::vector<std::string> mergeWithSettingsFile(const std::vector<std::string>& originalArgs);

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
            std::optional<Value> defaultValue,
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
		 * @param args Vector of command line arguments, e.g. {"--name=value", "--option", "value"}.
         */
        static void parseCommandLine(const std::vector<std::string>& args);

        /**
         * @brief Retrieves the typed value of a setting.
         * @tparam T Expected type: std::string or int.
         * @param name Name of the parameter.
         * @return Typed value of the parameter.
         */
        template<typename T>
        static T get(const std::string& name) {
            std::string key = QaplaHelpers::to_lowercase(name);
            auto it = values_.find(key);
            if (it == values_.end()) {
                auto defIt = definitions_.find(key);
                if (defIt == definitions_.end() || !defIt->second.defaultValue) {
                    throw std::runtime_error("Access to undefined setting: " + name);
                }

                return std::get<T>(*defIt->second.defaultValue);
            }
            return std::get<T>(it->second);
        }

        /**
         * @brief Get a configuration group by name.
         * @param groupName Name of the group (e.g. "engine").
         * @return List of instances for this group
         * @throws std::runtime_error if the group is unknown.
         */
        [[nodiscard]] static GroupInstances getGroupInstances(const std::string& groupName);


        static std::optional<GroupInstance> getGroupInstance(const std::string& groupName);

        /**
		 * @brief Displays help information for all registered settings and groups.
         */
        static void showHelp();

        /**
         * @brief Sets a global CLI setting programmatically (e.g., from interactive input).
         * @param name The parameter name (must match a registered global setting).
         * @param value The value to assign, in string form.
         * @return SetResult indicating success or error type.
         */
        static SetResult setGlobalValue(const std::string& name, const std::string& value);

        static void clearValues() {
            values_.clear();
            groupInstances_.clear();
        }

    private:

        struct ParsedParameter {
            std::string original;               // full raw input, for error reporting
            bool hasPrefix;                     // true if starts with "--"
            std::string name;                   // key part, never empty
            std::optional<std::string> value;   // optional value part
        };

        static Value parseBool(const ParsedParameter& arg);
        static Value parseInt(const ParsedParameter& arg);
        static Value parseUInt(const ParsedParameter& arg);
        static Value parseFloat(const ParsedParameter& arg);
        static Value parseString(const ParsedParameter& arg);
        static Value parsePathExists(const ParsedParameter& arg);
        static Value parsePathParentExists(const ParsedParameter& arg);

        /**
         * @brief Splits a raw command line argument into syntactic parts.
         * Does not perform semantic interpretation.
         * @param raw The raw argument string, e.g. "--foo=bar" or "baz".
         * @return ParsedParameter with decomposed components.
         */
        static ParsedParameter parseParameter(const std::string& raw);

        /**
         * @brief Parses an INI-like settings stream into CLI-style arguments.
         * Section headers (e.g. [engine]) create group switches (--engine),
         * followed by individual settings as positional parameters.
         * Top-level keys (without section) are parsed as --key value.
         * @param input The input stream to parse.
         * @return Vector of parsed arguments.
         */
        static std::vector<std::string> parseStreamToArgv(std::istream& input);


        /**
         * @brief Parses a single global parameter at the given position.
         * @param index Current index in args vector.
		 * @param args Vector of command line arguments.
         * @return Index of the next unprocessed argument.
         */
        static size_t parseGlobalParameter(size_t index, const std::vector<std::string>& args);

        /**
         * @brief Parses an entire grouped parameter block starting at the given position.
         * @param index Current index in args vector.
         * @param args Vector of command line arguments.
         * @return Index of the next unprocessed argument after the group block.
         */
        static size_t parseGroupedParameter(size_t index, const std::vector<std::string>& args);

        /**
         * @brief Looks up a key definition in a group, supporting suffix wildcard match like option.X.
         * @param group The group definition to search.
         * @param name The key to resolve (e.g. option.Hash).
         * @return Pointer to matching definition, or nullptr if not found.
         */
        static const Definition* resolveGroupedKey(const GroupDefinition& group, const std::string& name);

        /** 
         * @brief Validates that the given default value matches the expected ValueType.
         * Throws AppError if the type does not match or is semantically invalid.
         * @param name The name of the setting.
         * @param value The value to validate.
         * @param type The expected type.
         */
        static void validateDefaultValue(const std::string& name, const Value& value, ValueType type);

        /**
         * @brief Validates and finalizes all global parameters after parsing.
         * Throws if required values are missing.
         */
        static void finalizeGlobalParameters();

		/**
		 * @brief Parses a single value from a command line argument.
		 * @param arg The parsed parameter containing the name and optional value.
		 * @param def The definition of the expected parameter.
		 * @return The parsed Value, or throws if invalid.
		 */
        static Value parseValue(const ParsedParameter& arg, const Definition& def);


		// Static storage for definitions and group definitions
        static inline std::unordered_map<std::string, Definition> definitions_;
        static inline std::unordered_map<std::string, GroupDefinition> groupDefs_;

		// Static storage for parsed values and group instances
        static inline ValueMap values_;
        /**
         * @brief Stores grouped CLI values, organized by group name and ordered appearance.
         *        Each group name maps to a list of key-value maps (i.e., multiple blocks per group).
         */
        static inline GroupInstancesMap groupInstances_;

        
    };
} // namespace QaplaTester::CliSettings
