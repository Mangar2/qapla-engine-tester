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

#include "engine-capability.h"

#include "engine-option.h"
#include "../base-elements/string-helper.h"
#include "../base-elements/ini-file.h"

#include <stdexcept>
#include <format>

using namespace QaplaConfiguration;

using QaplaTester::EngineOption;
using QaplaTester::parseEngineProtocol;
using QaplaTester::EngineProtocol;

 /**
  * @brief Converts an EngineOption to a JSON-Line formatted string.
  * @param option The EngineOption to convert.
  * @return A JSON-Line formatted string representing the EngineOption.
  */
static std::string to_string(const EngineOption& option) {
    std::string json = std::format(R"({{"name": "{}", "type": "{}"")", option.name, EngineOption::to_string(option.type));

    if (!option.defaultValue.empty()) {
        json += std::format(R"(, "defaultValue": "{}")", option.defaultValue);
    }

    if (option.min.has_value()) {
        json += std::format(R"(, "min": {})", option.min.value());
    }

    if (option.max.has_value()) {
        json += std::format(R"(, "max": {})", option.max.value());
    }

    if (!option.vars.empty()) {
        json += R"(, "vars": [)";
        std::string delimiter;
        for (const auto& var : option.vars) {
            json += std::format(R"({}"{}")", delimiter, var);
            delimiter = ", ";
        }
        json += "]";
    }

    json += "}";
    return json;
}

/**
 * @brief Removes surrounding quotes from a string if present.
 */
std::string removeQuotes(std::string str) {
    if (str.front() == '"' && str.back() == '"') {
        str = str.substr(1, str.size() - 2);
    }
    return str;
}

/**
 * @brief Splits a string into tokens, treating special characters (e.g., '{', '}', ':', ',') as separate tokens.
 * @param str The input string to tokenize.
 * @return A vector of tokens extracted from the input string.
 */
static std::vector<std::string> tokenize(const std::string& str) {
    std::vector<std::string> tokens;
    std::string currentToken;
    bool insideString = false;
    char lastChar = '\0';

    for (char c : str) {
        if (c == '"' && lastChar != '\\') {
            insideString = !insideString;
        }
        else if (!insideString && (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',')) {
            // If we encounter a special character outside a string
            if (!currentToken.empty()) {
                tokens.push_back(QaplaHelpers::trim(currentToken));
                currentToken.clear();
            }
            tokens.push_back(std::string(1, c)); // Add the special character as a token
        }
        else {
            currentToken += c;
        }

        lastChar = c;
    }

    // Add the last token (if any)
    if (!currentToken.empty()) {
        tokens.push_back(QaplaHelpers::trim(currentToken));
    }

    return tokens;
}

static void parseVars(QaplaTester::EngineOption& option, const std::vector<std::string>& tokens, size_t& i) {
    if (tokens[i] != "[") {
        throw std::invalid_argument("Invalid JSON format: 'vars' must start with '['.");
    }
    i++;
    // Parse the array
    while (i < tokens.size() && tokens[i] != "]") {
        if (tokens[i] != ",") {
            option.vars.push_back(tokens[i]);
        }
        ++i;
    }

    if (i >= tokens.size() || tokens[i] != "]") {
        throw std::invalid_argument("Invalid JSON format: 'vars' array not properly closed.");
    }
}

static EngineOption parseEngineOption(const std::string& json) {
    EngineOption option;

    // Tokenize the input JSON string
    auto tokens = tokenize(json);

    // Ensure the JSON starts and ends with curly braces
    if (tokens.empty() || tokens.front() != "{" || tokens.back() != "}") {
        throw std::invalid_argument("Invalid JSON format: Missing opening or closing braces.");
    }

    size_t i = 1; 
    while (i < tokens.size() && tokens[i] != "}") { 
		const auto& currentKey = tokens[i];
        i++;
        if (i >= tokens.size() || tokens[i] != ":") {
            throw std::invalid_argument(std::format("Invalid JSON format: Expected ':' after key '{}'.", currentKey));
		}
		i++; 
        if (i >= tokens.size()) {
			throw std::invalid_argument(std::format("Invalid JSON format: Expected value after ':' for key '{}'.", currentKey));
        }
		const auto& value = tokens[i];
        i++;
        
        // Process the value for the current key
        if (currentKey == "name") {
            option.name = value;
        }
        else if (currentKey == "type") {
            option.type = EngineOption::parseType(value);
        }
        else if (currentKey == "defaultValue") {
            option.defaultValue = value;
        }
        else if (currentKey == "min") {
            option.min = std::stoi(value);
        }
        else if (currentKey == "max") {
            option.max = std::stoi(value);
        }
        else if (currentKey == "vars") {
            parseVars(option, tokens, --i);
            i++;
        }
        
        if (i < tokens.size() && tokens[i] == ",") {
            i++;
        }
    }

    return option;
}

/**
 * @brief Saves the engine capability data to a stream in INI format.
 * @param out The output stream to write the data to.
 */
void EngineCapability::save(std::ostream& out) const {
    // Write the section header
    out << "[enginecapability]" << std::endl;

    // Write the path
    out << "path=" << path_ << std::endl;

    // Write the protocol
    out << "protocol=" << to_string(protocol_) << std::endl;

    // Write the name
    out << "name=" << name_ << std::endl;

    // Write the author
    out << "author=" << author_ << std::endl;

    // Write the supported options
    for (const auto& option : supportedOptions_) {
        out << "option." << option.name << "=" << to_string(option) << std::endl;
    }
	// Add a blank line at the end for separation
    out << "\n";
}

EngineCapability EngineCapability::createFromSection(const QaplaHelpers::IniFile::Section& section) {
    EngineCapability capability;

    for (const auto& [key, value] : section.entries) {
        if (key == "path") {
            if (value.empty()) {
                throw std::invalid_argument("The 'path' value cannot be empty.");
            }
            capability.setPath(value);
        }
        else if (key == "protocol") {
            try {
                capability.setProtocol(parseEngineProtocol(value));
            }
            catch (const std::exception&) {
                throw std::invalid_argument(std::format("Invalid 'protocol' value: {}", value));
            }
        }
        else if (key == "name") {
            capability.setName(value);
        }
        else if (key == "author") {
            capability.setAuthor(value);
        }
        else if (key.rfind("option.", 0) == 0) {
            try {
                capability.supportedOptions_.push_back(parseEngineOption(value));
            }
            catch (const std::exception&) {
                // Ignore invalid options
                continue;
            }
        }
        else {
            // Ignore unknown keys
            continue;
        }
    }

    // Ensure required fields are set
    if (capability.getPath().empty()) {
        throw std::invalid_argument("Missing required 'path'.");
    }
    if (capability.getProtocol() == EngineProtocol::Unknown) {
        throw std::invalid_argument("Missing required 'protocol'.");
    }

    return capability;
}
