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
        std::string key = normalize(name);
        auto it = values.find(key);
        if (it == values.end()) {
            std::cerr << "Missing required setting: " << name << std::endl;
            std::exit(1);
        }
        return std::get<T>(it->second);
    }

private:
    struct SettingDefinition {
        std::string description;
        bool isRequired;
        Value defaultValue;
        ValueType type;
    };

    static std::unordered_map<std::string, SettingDefinition> definitions;
    static std::unordered_map<std::string, Value> values;

    static std::string normalize(const std::string& name);
    static void showHelp();
    static Value parseValue(const std::string& input, const SettingDefinition& def);
};
