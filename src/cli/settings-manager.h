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
#include <vector>
#include "../base-elements/app-error.h"
#include "../base-elements/string-helper.h"
#include "../base-elements/ini-file.h"
#include "../base-elements/stable-map.h"

namespace QaplaTester::Settings {

    enum class ValueType : std::uint8_t { String, Int, UInt, Float, Bool, PathExists, ValidateOutputPath };
    using Value = std::variant<std::string, int, unsigned int, bool, double>;
    using ValueMap = std::unordered_map<std::string, Value>;
    
    /**
     * @brief Definition of a global setting parameter.
     * Used as input configuration when registering a global setting with registerSetting().
     * Contains all metadata needed to validate and process the setting.
     */
    struct GlobalDefinition {
        std::string name;                    ///< Parameter name, case-insensitive
        std::string description;             ///< Help text shown to users
        std::string longDescription{};         ///< Detailed AI-oriented description for MCP
        bool isRequired = false;             ///< True if parameter must be provided
        std::optional<Value> defaultValue;   ///< Default value if parameter is not required
        ValueType type;                      ///< Expected value type for validation        
        bool isHidden = false;               ///< True if parameter should not be shown in help output    
    };
    
    /**
     * @brief Definition of a parameter within a group.
     * Describes a single key-value parameter that can appear in a grouped section.
     * The parameter name is stored as the key in the GroupDefinition::keys map.
     */
    struct ParameterDefinition {
        std::string description;             ///< Help text shown to users
        std::string longDescription{};       ///< Detailed AI-oriented description for MCP
        bool isRequired = false;             ///< True if parameter must be provided within its group
        std::optional<Value> defaultValue;   ///< Default value if parameter is not required
        ValueType type;                      ///< Expected value type for validation
        bool exclusive = false;              ///< True if parameter is mutually exclusive with others in the group        
        bool isHidden = false;               ///< True if parameter should not be shown in help output    
    };

    /**
     * @brief Definition of a grouped parameter section.
     * Used as input configuration when registering a group with registerGroup().
     * Defines a reusable parameter block (e.g., --engine) that can contain multiple key-value pairs.
     */
    struct GroupDefinition {
        std::string name;                                             ///< Group name, case-insensitive (e.g., "engine", "openings")
        std::string description;                                      ///< Help text shown to users
        std::string longDescription{};                                  ///< Detailed AI-oriented description for MCP
        bool unique;                                                  ///< True if only one instance of this group is allowed
        std::vector<std::string> primaryKey{};                          ///< List of keys that uniquely identify an instance of the group
        QaplaHelpers::StableMap<std::string, ParameterDefinition> keys;   ///< Map of parameter names to their definitions (preserves insertion order)
        bool ignore = false;                                          ///< True if this group should be ignored during parsing and validation

        /**
         * @brief Returns all defined keys in the order they were registered.
         * @return Vector of key names in definition order.
        */
        [[nodiscard]] std::vector<std::string> getKeyNames() const {
            std::vector<std::string> result;
            result.reserve(keys.size());
            for (const auto& [key, _] : keys) {
                result.push_back(key);
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

        [[nodiscard]] bool isKeyProvided(const std::string& name) const {
            std::string key = QaplaHelpers::to_lowercase(name);
            return values_.contains(key);
        }

        [[nodiscard]] const GroupDefinition& getDefinition() const {
            return definition_;
        }

        [[nodiscard]] const ValueMap& getValues() const {
			return values_;
		}

        /**
         * @brief Sets a value in the group instance.
         */
        void setValue(const std::string& key, const std::string& value) {
            std::string k = QaplaHelpers::to_lowercase(key);
            values_[k] = value;
        }

        /**
         * @brief Merges another GroupInstance into this one, using it as default values.
         * Values from this instance take precedence (like std::map::insert behavior).
         * Use this to apply defaults while preserving existing values.
         * @param other The GroupInstance containing default values.
         * @return A new GroupInstance with merged values.
         */
        [[nodiscard]] GroupInstance mergeWithDefaults(const GroupInstance& other) const {
            ValueMap merged = values_;
            const auto& otherValues = other.getValues();
            merged.insert(otherValues.begin(), otherValues.end());
            return { merged, definition_ };
        }

    private:
        ValueMap values_;
        GroupDefinition definition_;
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
         * @brief Returns the singleton instance of Manager.
         */
        [[nodiscard]] static Manager& instance() {
            static Manager inst;
            return inst;
        }

        /**
         * @brief Registers a setting with its metadata.
         * @param config Configuration struct containing all setting parameters.
         */
        void registerSetting(const GlobalDefinition& config);

        /**
         * @brief Registers a grouped CLI block (e.g. --engine key=value...) with description and expected keys.
         * @param config Configuration struct containing group parameters including keys.
         */
        void registerGroup(const GroupDefinition& config);

        /**
         * @brief Parses configuration data from ConfigData structure.
		 * @param configData ConfigData instance containing configuration sections and parameters.
         * @param overwrite When true, existing values are overwritten. When false, existing values are kept.
         * @param strict When true, unknown sections throw errors. When false, unknown sections are silently ignored.
         */
        void parseInput(const QaplaHelpers::ConfigData& configData, bool overwrite = false, bool strict = true);

        /**
         * @brief Validates all group instances for completeness and adds missing defaults.
         * Must be called after all parseInput and mergeSectionList operations are complete.
         * Checks for required parameters and adds default values where needed.
         * @throws AppError if required parameters are missing.
         */
        void validateGroupCompleteness();

        /**
         * @brief Validates completeness of all settings (global and grouped) after parsing.
         * Applies default values for missing optional parameters and checks for required parameters.
         * @throws AppError if required parameters are missing.
         */
        void validateCompleteness();

        /**
         * @brief Retrieves the typed value of a setting.
         * @tparam T Expected type: std::string or int.
         * @param name Name of the parameter.
         * @return Typed value of the parameter.
         */
        template<typename T>
        T get(const std::string& name) {
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
         * @brief Checks if a global parameter was provided by the user.
         * @param name Name of the parameter.
         * @return True if the parameter was explicitly provided, false otherwise.
         */
        [[nodiscard]] bool isKeyProvided(const std::string& name) const {
            std::string key = QaplaHelpers::to_lowercase(name);
            return values_.contains(key);
        }

        /**
         * @brief Get a configuration group by name.
         * @param groupName Name of the group (e.g. "engine").
         * @return List of instances for this group
         * @throws std::runtime_error if the group is unknown.
         */
        [[nodiscard]] GroupInstances getGroupInstances(const std::string& groupName);

        [[nodiscard]] GroupInstances& getGroupInstancesMutable(const std::string& groupName);
        void setGroupInstances(const std::string& groupName, const GroupInstances& instances);

        [[nodiscard]] std::optional<GroupInstance> getGroupInstance(const std::string& groupName);

        /**
		 * @brief Displays help information for all registered settings and groups.
         */
        void showHelp();

        /**
         * @brief Sets a global CLI setting programmatically (e.g., from interactive input).
         * @param name The parameter name (must match a registered global setting).
         * @param value The value to assign, in string form.
         * @return SetResult indicating success or error type.
         */
        SetResult setGlobalValue(const std::string& name, const std::string& value);

        /**
         * @brief Clears all stored settings and group instances.
         * Resets the Manager to an empty state.
         */
        void clearValues() {
            values_.clear();
            groupInstances_.clear();
        }

        /**
         * @brief Clears all instances of a specific group.
         * Useful for resetting active configurations (e.g. engines) without affecting globals.
         * @param groupName Name of the group to clear.
         */
        void clearGroup(const std::string& groupName) {
            auto name = QaplaHelpers::to_lowercase(groupName);
            groupInstances_.erase(name);
        }

        /**
         * @brief Modifies or duplicates an existing group instance.
         * Finds a group instance matching the 'search' criteria, creates a copy of its data,
         * applies the changes from 'replace', and re-submits it for parsing/validation.
         * If 'replace' contains new primary keys, this acts as a 'Copy'.
         * If 'replace' keeps primary keys, this acts as an 'Update'.
         * 
         * @param search Section defining the group name and criteria to find the source instance.
         * @param replace Section containing values to overwrite in the source data.
         * @throws AppError if the source instance cannot be found.
         */
        void modifyGroupInstance(const QaplaHelpers::IniFile::Section& search,
                                 const QaplaHelpers::IniFile::Section& replace);

        /**
         * @brief Removes group instances that match the specified criteria.
         * @param groupName Name of the group.
         * @param criteria Key-value pairs to match against.
         * @return Number of removed instances.
         */
        size_t removeGroupInstances(const std::string& groupName, 
                                    const QaplaHelpers::IniFile::KeyValueMap& criteria);

        /**
         * @brief Converts settings to ConfigData for a specific set of section names.
         * Includes global parameters and group instances.
         * Only outputs entries that are mandatory or differ from their default values.
         * @param sectionNames Names of sections to include. Empty vector means all sections.
         * @param addGlobals If true, includes global parameters in the result.
         * @return ConfigData instance containing the filtered settings.
         */
        [[nodiscard]] QaplaHelpers::ConfigData toConfigData(
                const std::vector<std::string>& sectionNames = {},
                bool addGlobals = true) const;

        [[nodiscard]] const std::unordered_map<std::string, ParameterDefinition>& getDefinitions() const { return definitions_; }
        [[nodiscard]] const std::unordered_map<std::string, GroupDefinition>& getGroupDefinitions() const { return groupDefs_; }

    private:

        /**
         * @brief Converts group instances of a specific name to a list of Sections.
         * @param groupName Name of the group to convert (e.g., "engine").
         * @param suppressDefault If true, only includes entries that are mandatory or differ from their default values.
         * @return SectionList containing all instances of the specified group.
         */
        [[nodiscard]] QaplaHelpers::IniFile::SectionList groupInstancesToSectionList(
                const std::string& groupName, bool suppressDefault = false) const;

        /**
         * @brief Collects global parameters that are mandatory or differ from their default values.
         * @return KeyValueMap containing filtered global parameters.
         */
        [[nodiscard]] QaplaHelpers::IniFile::KeyValueMap getFilteredGlobalParameters() const;

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
        static Value parsePathIsValid(const ParsedParameter& arg);

        /**
         * @brief Splits a raw command line argument into syntactic parts.
         * Does not perform semantic interpretation.
         * @param raw The raw argument string, e.g. "--foo=bar" or "baz".
         * @return ParsedParameter with decomposed components.
         */
        static ParsedParameter parseParameter(const std::string& raw);

        /**
         * @brief Checks for help request in global parameters and exits if found.
         * @param configData ConfigData instance to check for help parameter.
         */
        void handleHelpRequest(const QaplaHelpers::ConfigData& configData);

        /**
         * @brief Processes all sections in the config data.
         * @param configData ConfigData instance containing configuration sections.
         * @param overwrite When true, existing values are overwritten. When false, existing values are kept.
         * @param strict When true, unknown sections throw errors. When false, unknown sections are silently ignored.
         */
        void processSections(const QaplaHelpers::ConfigData& configData, bool overwrite, bool strict);

        /**
         * @brief Processes a section map containing multiple section lists.
         * @param sectionMap Map of section IDs to section lists.
         * @param overwrite When true, existing values are overwritten. When false, existing values are kept.
         * @param strict When true, unknown sections throw errors. When false, unknown sections are silently ignored.
         */
        void processSectionMap(const QaplaHelpers::ConfigData::SectionMap& sectionMap, bool overwrite, bool strict);



        /**
         * @brief Parses a single global parameter from a section entry.
         * @param key Parameter key.
         * @param value Parameter value.
         * @param overwrite When true, existing values are overwritten. When false, existing values are kept.
         */
        void parseGlobalParameter(const std::string& key, const std::string& value, bool overwrite);

        /**
         * @brief Parses an entire grouped parameter section.
         * @param section The section to parse.
         * @param overwrite When true, existing values are overwritten. When false, existing values are kept.
         * @param strict When true, unknown sections throw errors. When false, unknown sections are silently ignored.
         */
        void parseGroupedParameter(const QaplaHelpers::IniFile::Section& section, bool overwrite, bool strict);

        static std::string valueToString(const Value& value);

        [[nodiscard]] const GroupInstance* findGroupInstance(const std::string& groupName, 
                                                            const QaplaHelpers::IniFile::KeyValueMap& criteria) const;
        
        [[nodiscard]] static QaplaHelpers::IniFile::Section groupInstanceToSection(const std::string& sectionName, 
                                                                           const GroupInstance& instance);

        /**
         * @brief Looks up a key definition in a group, supporting suffix wildcard match like option.X.
         * @param group The group definition to search.
         * @param name The key to resolve (e.g. option.Hash).
         * @return Pointer to matching definition, or nullptr if not found.
         */
        static const ParameterDefinition* resolveGroupedKey(
            const GroupDefinition& group, const std::string& name);

        /**
         * @brief Parses all entries in a section and returns a ValueMap.
         * @param section The section containing entries to parse.
         * @param groupDefinition The group definition for validation.
         * @param overwrite When true, existing keys in existingMap are overwritten. When false, existing keys are kept.
         * @param existingMap Optional pointer to existing ValueMap to use as base.
         * @return ValueMap containing parsed entries, optionally merged with existing values.
         */
        [[nodiscard]] static ValueMap parseSectionEntries(
            const QaplaHelpers::IniFile::Section& section, 
            const GroupDefinition& groupDefinition,
            bool overwrite,
            const ValueMap* existingMap = nullptr);

        /**
         * @brief Validates that no exclusive keys are combined with other keys.
         * @param group The parsed group values to validate.
         * @param groupDefinition The group definition containing exclusivity rules.
         * @param sectionName The section name for error reporting.
         * @throws AppError if exclusive keys are combined with others.
         */
        static void validateExclusiveKeys(const ValueMap& group, 
            const GroupDefinition& groupDefinition, 
            const std::string& sectionName);

        /** 
         * @brief Validates that the given default value matches the expected ValueType.
         * Throws AppError if the type does not match or is semantically invalid.
         * @param name The name of the setting.
         * @param value The value to validate.
         * @param type The expected type.
         */
        static void validateDefaultValue(const std::string& name, const Value& value, ValueType type);

        /**
         * @brief Checks if a Section matches an existing GroupInstance by comparing primaryKey values.
         * @param section The section containing the values to check.
         * @param instance The existing GroupInstance to compare against.
         * @param primaryKeys The list of key names that must match.
         * @return True if all primaryKey values match, false otherwise.
         */
        static bool matchesPrimaryKey(const QaplaHelpers::IniFile::Section& section,
            const GroupInstance& instance,
            const std::vector<std::string>& primaryKeys);

        /**
         * @brief Finds a GroupInstance in a list by matching primaryKey values from a Section.
         * @param instances The list of GroupInstances to search.
         * @param section The section containing primaryKey values to match.
         * @param primaryKeys The list of key names that must match.
         * @return Index to the matching GroupInstance
         */
        static std::optional<size_t> findInstanceByPrimaryKey(GroupInstances& instances,
            const QaplaHelpers::IniFile::Section& section,
            const std::vector<std::string>& primaryKeys);

        /**
         * @brief Validates and finalizes all global parameters after parsing.
         * Throws if required values are missing.
         */
        void validateGlobalParameterCompleteness();

		/**
		 * @brief Parses a single value from a command line argument.
		 * @param arg The parsed parameter containing the name and optional value.
		 * @param def The definition of the expected parameter.
		 * @return The parsed Value, or throws if invalid.
		 */
        static Value parseValue(const ParsedParameter& arg, const ParameterDefinition& def);


		// Storage for definitions and group definitions
        std::unordered_map<std::string, ParameterDefinition> definitions_;
        std::unordered_map<std::string, GroupDefinition> groupDefs_;

		// Storage for parsed values and group instances
        ValueMap values_;
        /**
         * @brief Stores grouped CLI values, organized by group name and ordered appearance.
         *        Each group name maps to a list of key-value maps (i.e., multiple blocks per group).
         */
        GroupInstancesMap groupInstances_;

        /**
         * @brief Converts a group instance to an IniFile::Section.
         * @param groupName The name of the group for the section.
         * @param instance The group instance to convert.
         * @param suppressDefault If true, keys that match their default values are omitted.
         * @return The converted section.
         */
        [[nodiscard]] static QaplaHelpers::IniFile::Section instanceToSection(
            const std::string& groupName, const GroupInstance& instance, bool suppressDefault);
        
    };
} // namespace QaplaTester::Settings
