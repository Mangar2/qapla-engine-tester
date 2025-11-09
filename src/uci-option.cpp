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

#include "uci-option.h"
#include "engine-option.h"
#include <sstream>
#include <stdexcept>

namespace QaplaTester {

namespace {

void parseKeyValuePairs(std::istringstream& iss, EngineOption& opt) {
    std::string key;
    std::string valueBuffer;

    auto flushValue = [&]() {
        if (key == "name") {
            opt.name = std::move(valueBuffer);
        }
        else if (key == "type") {
            opt.type = EngineOption::parseType(valueBuffer);
        }
        else if (key == "default") {
            opt.defaultValue = std::move(valueBuffer);
        }
        else if (key == "min") {
            opt.min = std::stoi(valueBuffer);
        }
        else if (key == "max") {
            opt.max = std::stoi(valueBuffer);
        }
        else if (key == "var") {
            opt.vars.push_back(std::move(valueBuffer));
        }
        key.clear();
        valueBuffer.clear();
    };

    std::string token;
    while (iss >> token) {
        if (token == "name" || token == "type" || token == "default" ||
            token == "min" || token == "max" || token == "var") {
            if (!key.empty()) {
                flushValue();
            }
            key = token;
        }
        else {
            if (!valueBuffer.empty()) { valueBuffer += ' '; }
            valueBuffer += token;
        }
    }
    if (!key.empty()) {
        flushValue();
    }
}

void validateOption(const EngineOption& opt, const std::string& line) {
    if (opt.name.empty()) {
        throw std::runtime_error("Missing 'name' field in option line: " + line);
    }
    if (opt.type == EngineOption::Type::Unknown) {
        throw std::runtime_error("Missing or unknown 'type' field in option line: " + line);
    }

    // Type-dependent structural validation
    switch (opt.type) {
    case EngineOption::Type::Spin:
        if (!opt.min.has_value() || !opt.max.has_value()) {
            throw std::runtime_error("Spin option missing min or max: " + opt.name);
        }
        break;
    case EngineOption::Type::Combo:
        if (opt.vars.empty()) {
            throw std::runtime_error("Combo option missing var entries: " + opt.name);
        }
        break;
    case EngineOption::Type::Button:
    case EngineOption::Type::Check:
    case EngineOption::Type::String:
        if (opt.min || opt.max) {
            throw std::runtime_error("Option type '" + EngineOption::to_string(opt.type) + "' must not define min/max: " + opt.name);
        }
        break;
    default:
        break;
    }
}

} // anonymous namespace

EngineOption parseUciOptionLine(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;
    if (token != "option") {
        throw std::runtime_error("Line does not start with 'option': " + line);
    }

    EngineOption opt;
    parseKeyValuePairs(iss, opt);
    validateOption(opt, line);

    return opt;
}

} // namespace QaplaTester