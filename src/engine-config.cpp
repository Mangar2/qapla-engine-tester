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
 
#include "engine-config.h"

std::unordered_map<std::string, std::string> EngineConfig::getOptions(const EngineOptions availableOptions) const {
    std::unordered_map<std::string, std::string> filteredOptions;
    for (const auto& [key, value] : optionValues) {
        if (availableOptions.find(key) != availableOptions.end()) {
            filteredOptions[key] = value;
        }
    }
    return filteredOptions;
}

/**
 * @brief Sets multiple options at once from a map of key-value pairs coming from the command line
 * @param values A map of option names and their values.
 * @throw std::runtime_error if a required field is missing or an unknown key is encountered.
 */
void EngineConfig::setCommandLineOptions(const std::unordered_map<std::string, std::string>& values) {
    std::unordered_set<std::string> seenKeys;

    for (const auto& [key, value] : values) {
        if (!seenKeys.insert(key).second)
            throw std::runtime_error("Duplicate key in engine options: " + key);

        if (key == "name") setName(value);
        else if (key == "cmd") setExecutablePath(value);
        else if (key == "dir") setWorkingDirectory(value);
        else if (key == "proto") {
            if (value == "uci") protocol = EngineProtocol::Uci;
            else if (value == "xboard") protocol = EngineProtocol::XBoard;
            else throw std::runtime_error("Unknown protocol: " + value);
        }
        else if (key.starts_with("option.")) {
            optionValues[key.substr(7)] = value;
        }
        else {
            throw std::runtime_error("Unknown engine option key: " + key);
        }
    }

    if (getName().empty()) throw std::runtime_error("Missing required field: name");
    if (getExecutablePath().empty()) throw std::runtime_error("Missing required field: cmd");
    if (protocol == EngineProtocol::Unknown) throw std::runtime_error("Missing or invalid protocol");
}


std::istream& operator>>(std::istream& in, EngineConfig& config) {
    std::string line;
    bool sectionRead = false;
    std::unordered_set<std::string> seenKeys;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#' || line[0] == ';') continue;

        if (!sectionRead) {
            if (line.front() == '[' && line.back() == ']') {
                std::string name = line.substr(1, line.size() - 2);
                if (name.empty()) throw std::runtime_error("Empty section name");
                config.setName(name);
                sectionRead = true;
                continue;
            }
            else {
                throw std::runtime_error("Expected section header");
            }
        }

        if (line.front() == '[' && line.back() == ']') {
            // Neue Section gefunden -> Stream zurücksetzen
            in.seekg(-(std::streamoff)line.length() - 1, std::ios_base::cur);
            break;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        if (!seenKeys.insert(key).second)
            throw std::runtime_error("Duplicate key: " + key);

        if (key == "name") config.setName(value);
        else if (key == "executablePath") config.setExecutablePath(value);
        else if (key == "workingDirectory") config.setWorkingDirectory(value);
        else if (key == "protocol") {
            if (value == "uci") config.protocol = EngineProtocol::Uci;
            else if (value == "xboard") config.protocol = EngineProtocol::XBoard;
            else throw std::runtime_error("Unknown protocol: " + value);
        }
        else config.setOptionValue(key, value);
    }

    if (!sectionRead)
        throw std::runtime_error("Missing section header");
    if (config.getExecutablePath().empty())
        throw std::runtime_error("Missing required field: executablePath");
    if (config.protocol == EngineProtocol::Unknown)
        throw std::runtime_error("Missing required field: protocol");

    return in;
}


std::ostream& operator<<(std::ostream& out, const EngineConfig& config) {
    if (!out) throw std::runtime_error("Invalid output stream");

    out << "[" << config.name << "]\n";
    out << "protocol=" << to_string(config.protocol) << '\n';
    out << "executablePath=" << config.executablePath << '\n';
    out << "workingDirectory=" << config.workingDirectory << '\n';

    for (const auto& [key, value] : config.optionValues) {
        out << key << "=" << value << '\n';
    }

    return out;
}
