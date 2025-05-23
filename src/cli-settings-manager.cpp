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

using namespace std;

unordered_map<string, CliSettingsManager::SettingDefinition> CliSettingsManager::definitions;
unordered_map<string, CliSettingsManager::Value> CliSettingsManager::values;

void CliSettingsManager::registerSetting(const string& name,
    const string& description,
    bool isRequired,
    Value defaultValue,
    ValueType type) {
    string key = normalize(name);
    definitions[key] = { description, isRequired, defaultValue, type };
}

void CliSettingsManager::parseCommandLine(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--help") {
            showHelp();
            exit(0);
        }
        auto eqPos = arg.find('=');
        if (arg.rfind("--", 0) == 0 && eqPos != string::npos) {
            string name = arg.substr(2, eqPos - 2);
            string value = arg.substr(eqPos + 1);
            string key = normalize(name);
            auto defIt = definitions.find(key);
            if (defIt != definitions.end()) {
                values[key] = parseValue(value, defIt->second);
            }
            else {
				throw std::runtime_error("Unknown parameter: " + name);
            }
        }
        else {
			throw std::runtime_error("Invalid argument format: " + arg);

        }
    }

    for (const auto& [key, def] : definitions) {
        if (values.find(key) != values.end())
            continue;

        // Fall 2: Required ohne Default -> Eingabe erzwingen
        if (def.isRequired && def.defaultValue.valueless_by_exception()) {
            std::string input;
            std::cout << key << " (required): ";
            std::getline(std::cin, input);
            values[key] = parseValue(input, def);
            continue;
        }

        // Fall 4: Optional mit Default -> direkt setzen, keine Eingabe
        if (!def.isRequired && !def.defaultValue.valueless_by_exception()) {
            values[key] = def.defaultValue;
            continue;
        }

        // Fall 3: Required mit Default -> Eingabe mit Default-Vorschlag
        if (def.isRequired && !def.defaultValue.valueless_by_exception()) {
            std::string inputPrompt = key + " (required, default: ";
            std::visit([&inputPrompt](auto&& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, int>)
                    inputPrompt += std::to_string(v);
                else
                    inputPrompt += v;
                }, def.defaultValue);
            inputPrompt += "): ";

            std::string input;
            std::cout << inputPrompt;
            std::getline(std::cin, input);
            values[key] = input.empty() ? def.defaultValue : parseValue(input, def);
            continue;
        }

        // Fall 5: Optional ohne Default -> Fehler im Code
		throw std::runtime_error("Invalid setting: optional parameter '" + key + "' without default value.");
    }
}

string CliSettingsManager::normalize(const string& name) {
    string lower = name;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

void CliSettingsManager::showHelp() {
    cout << "Available options:\n";
    for (const auto& [key, def] : definitions) {
        cout << "  --" << key << "=";
        if (def.type == ValueType::Int) cout << "<int>";
        else cout << "<string>";
        cout << "\t" << def.description;
        if (def.isRequired) {
            cout << " [required]";
        }
        else {
            cout << " (default: ";
            visit([](auto&& v) { cout << v; }, def.defaultValue);
            cout << ")";
        }
        cout << "\n";
    }
}

CliSettingsManager::Value CliSettingsManager::parseValue(const string& input, const SettingDefinition& def) {
    if (def.type == ValueType::Int) {
        try {
            return stoi(input);
        }
        catch (...) {
			throw std::runtime_error("Invalid integer: " + input);
        }
    }

    if (def.type == ValueType::PathExists) {
        if (!filesystem::exists(input)) {
			throw std::runtime_error("Path does not exist: " + input);
        }
    }

    return input;
}
