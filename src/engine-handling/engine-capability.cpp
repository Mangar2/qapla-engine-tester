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
#include "../base-elements/qapla-json.h"
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
    auto json = QaplaTester::Json::JsonValue::object();
    json["name"] = option.name;
    json["type"] = EngineOption::to_string(option.type);

    if (!option.defaultValue.empty()) {
        json["defaultValue"] = option.defaultValue;
    }
    if (option.min.has_value()) {
        json["min"] = static_cast<double>(*option.min);
    }
    if (option.max.has_value()) {
        json["max"] = static_cast<double>(*option.max);
    }
    if (!option.vars.empty()) {
        auto& vars = json["vars"] = QaplaTester::Json::JsonValue::array();
        for (const auto& var : option.vars) {
            vars.push_back(var);
        }
    }

    return json.stringify();
}

/**
 * @brief Parses a persisted JSON-Line capability entry into an EngineOption.
 *
 * Accepts both the compact format written by to_string() above and the
 * spaced format written by older versions of this file (read compatibility
 * for the persisted capability cache).
 */
static EngineOption parseEngineOption(const std::string& json) {
    const auto parsed = QaplaTester::Json::JsonValue::parse(json);
    if (!parsed.is_object()) {
        throw std::invalid_argument("Invalid JSON format: expected an object.");
    }

    EngineOption option;
    const auto& object = parsed.as_object();

    if (const auto it = object.find("name"); it != object.end() && it->second.is_string()) {
        option.name = it->second.as_string();
    }
    if (const auto it = object.find("type"); it != object.end() && it->second.is_string()) {
        option.type = EngineOption::parseType(it->second.as_string());
    }
    if (const auto it = object.find("defaultValue"); it != object.end() && it->second.is_string()) {
        option.defaultValue = it->second.as_string();
    }
    if (const auto it = object.find("min"); it != object.end() && it->second.is_number()) {
        option.min = static_cast<int>(it->second.as_number());
    }
    if (const auto it = object.find("max"); it != object.end() && it->second.is_number()) {
        option.max = static_cast<int>(it->second.as_number());
    }
    if (const auto it = object.find("vars"); it != object.end() && it->second.is_array()) {
        for (const auto& var : it->second.as_array()) {
            if (var.is_string()) {
                option.vars.push_back(var.as_string());
            }
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
