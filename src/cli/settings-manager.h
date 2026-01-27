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
#include "../base-elements/app-error.h"
#include "../base-elements/string-helper.h"
#include "../base-elements/ini-file.h"

namespace QaplaTester::Settings {

    enum class ValueType : std::uint8_t { String, Int, UInt, Float, Bool, PathExists, PathParentExists };
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
        bool unique;                                                  ///< True if only one instance of this group is allowed
        std::unordered_map<std::string, ParameterDefinition> keys;   ///< Map of parameter names to their definitions

        /**
         * @brief Returns all defined keys in the group. Keys ending with ".[name]" are returned without that suffix.
         * @return Vector of key names without wildcard suffixes.
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
         * @brief Merges another GroupInstance into this one.
         * Values from this instance take precedence (like std::map::insert behavior).
         * @param other The GroupInstance to merge from.
         * @return A new GroupInstance with merged values.
         */
        [[nodiscard]] GroupInstance merge(const GroupInstance& other) const {
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
         * @param strict When true, unknown parameters cause an error. When false, unknown parameters are ignored.
         */
        void parseInput(const QaplaHelpers::ConfigData& configData, bool strict = true);

        /**
         * @brief Merges a section list into existing settings.
         * @param sectionName Name of the section (e.g., "engine", "openings").
         * @param sections List of sections to merge.
         * @param mergeIdentifier Field name used to identify matching instances (e.g., "name" for engines). Empty for unique groups.
         * @param strict When true, unknown parameters cause an error. When false, unknown parameters are ignored.
         */
        void mergeSectionList(const std::string& sectionName,
                             const QaplaHelpers::IniFile::SectionList& sections,
                             const std::string& mergeIdentifier = "",
                             bool strict = true);

        /**
         * @brief Merges entire ConfigData into existing settings.
         * Processes all section names in the ConfigData, merging each section list using mergeSectionList.
         * @param configData ConfigData instance to merge.
         * @param strict When true, unknown parameters cause an error. When false, unknown parameters are ignored.
         */
        void mergeConfigData(const QaplaHelpers::ConfigData& configData, bool strict = true);

        /**
         * @brief Validates all group instances for completeness and adds missing defaults.
         * Must be called after all parseInput and mergeSectionList operations are complete.
         * Checks for required parameters and adds default values where needed.
         * @throws AppError if required parameters are missing.
         */
        void validateGroupCompleteness();

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
         * @brief Get a configuration group by name.
         * @param groupName Name of the group (e.g. "engine").
         * @return List of instances for this group
         * @throws std::runtime_error if the group is unknown.
         */
        [[nodiscard]] GroupInstances getGroupInstances(const std::string& groupName);


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

        void clearValues() {
            values_.clear();
            groupInstances_.clear();
        }

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

    private:

        /**
         * @brief Converts group instances of a specific name to an IniFile::SectionList.
         * Only includes entries that are mandatory or differ from their default values.
         * @param groupName Name of the group to convert (e.g., "engine").
         * @return SectionList containing all instances of the specified group.
         */
        [[nodiscard]] QaplaHelpers::IniFile::SectionList groupInstancesToSectionList(
                const std::string& groupName) const;

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
        static Value parsePathParentExists(const ParsedParameter& arg);

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
         * @param strict When true, unknown parameters cause an error.
         */
        void processSections(const QaplaHelpers::ConfigData& configData, bool strict);

        /**
         * @brief Processes a section map containing multiple section lists.
         * @param sectionMap Map of section IDs to section lists.
         * @param strict When true, unknown parameters cause an error.
         */
        void processSectionMap(const QaplaHelpers::ConfigData::SectionMap& sectionMap, bool strict);

        /**
         * @brief Processes a single section based on its type (global or grouped).
         * @param section The section to process.
         * @param strict When true, unknown parameters cause an error.
         */
        void processSection(const QaplaHelpers::IniFile::Section& section, bool strict);

        /**
         * @brief Processes all entries in a section as global parameters.
         * @param section The section containing entries to process.
         * @param strict When true, unknown parameters cause an error.
         */
        void processSectionEntries(const QaplaHelpers::IniFile::Section& section, bool strict);

        /**
         * @brief Parses a single global parameter from a section entry.
         * @param key Parameter key.
         * @param value Parameter value.
         * @param strict When true, unknown parameters cause an error. When false, unknown parameters are ignored.
         */
        void parseGlobalParameter(const std::string& key, const std::string& value, bool strict);

        /**
         * @brief Parses an entire grouped parameter section.
         * @param section The section to parse.
         * @param strict When true, unknown parameters cause an error. When false, unknown parameters are ignored.
         */
        void parseGroupedParameter(const QaplaHelpers::IniFile::Section& section, bool strict);

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
         * @param strict When true, unknown parameters cause an error.
         * @return ValueMap containing parsed entries.
         */
        [[nodiscard]] static ValueMap parseSectionEntries(
            const QaplaHelpers::IniFile::Section& section, 
            const GroupDefinition& groupDefinition, 
            bool strict);

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
         * @brief Merges or appends a new instance to the group instances.
         * @param groupName The name of the group.
         * @param newInstance The new instance to merge or append.
         * @param groupDefinition The group definition.
         * @param section The section containing the merge identifier.
         * @param mergeIdentifier The field name used to identify matching instances.
         */
        void mergeOrAppendInstance(const std::string& groupName,
            const GroupInstance& newInstance,
            const GroupDefinition& groupDefinition,
            const QaplaHelpers::IniFile::Section& section,
            const std::string& mergeIdentifier);

        /**
         * @brief Tries to merge a new instance with an existing one by identifier.
         * @param instances The existing instances to search and merge with.
         * @param newInstance The new instance to merge.
         * @param mergeIdentifier The field name used to identify matching instances.
         * @param newIdValue The identifier value from the new instance.
         * @return True if merge was successful, false otherwise.
         */
        static bool tryMergeByIdentifier(GroupInstances& instances,
            const GroupInstance& newInstance,
            const std::string& mergeIdentifier,
            const std::string& newIdValue);

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
        void finalizeGlobalParameters();

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

        
    };
} // namespace QaplaTester::CliSettings
