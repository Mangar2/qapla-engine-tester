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
 
#include "mcp-converter.h"
#include "../cli/settings-definitions.h"
#include "../base-elements/app-error.h"
#include <format>
#include <cmath>

namespace QaplaTester::Mcp {

namespace {

Settings::Value convertByType(const std::string& key, Settings::ValueType type, const JsonValue& jsonVal) {
    switch (type) {
        case Settings::ValueType::String:
        case Settings::ValueType::PathExists:
        case Settings::ValueType::ValidateOutputPath:
            if (!jsonVal.isString()) {
                throw AppError::makeInvalidParameters(std::format("Parameter '{}' expects a string value.", key));
            }
            return jsonVal.asString();

        case Settings::ValueType::Int:
            if (!jsonVal.isNumber()) {
                throw AppError::makeInvalidParameters(std::format("Parameter '{}' expects an integer value.", key));
            }
            return static_cast<int>(jsonVal.asDouble());

        case Settings::ValueType::UInt:
            if (!jsonVal.isNumber()) {
                throw AppError::makeInvalidParameters(std::format("Parameter '{}' expects an unsigned integer value.", key));
            }
            return static_cast<unsigned int>(jsonVal.asDouble());

        case Settings::ValueType::Float:
             if (!jsonVal.isNumber()) {
                throw AppError::makeInvalidParameters(std::format("Parameter '{}' expects a number value.", key));
            }
            return jsonVal.asDouble();

        case Settings::ValueType::Bool:
            if (!jsonVal.isBool()) {
                throw AppError::makeInvalidParameters(std::format("Parameter '{}' expects a boolean value.", key));
            }
            return jsonVal.asBool();
            
        default:
            throw AppError::makeInvalidParameters(std::format("Unsupported type for parameter '{}'.", key));
    }
}

std::string valueToString(const Settings::Value& val) {
    if (std::holds_alternative<std::string>(val)) {
        return std::get<std::string>(val);
    }
    if (std::holds_alternative<int>(val)) {
        return std::to_string(std::get<int>(val));
    }
    if (std::holds_alternative<unsigned int>(val)) {
        return std::to_string(std::get<unsigned int>(val));
    }
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val) ? "true" : "false";
    }
    if (std::holds_alternative<double>(val)) {
         double d = std::get<double>(val);
         if (std::floor(d) == d) {
             return std::to_string(static_cast<long long>(d));
         }
         return std::to_string(d);
    }
    return "";
}

} // namespace

Settings::Value convertJsonToEngineSetting(const std::string& key, const JsonValue& jsonVal) {
    // 1. Check strict schema definitions
    static const auto engineKeys = Settings::getEngineKeys();
    auto it = engineKeys.find(key);

    if (it != engineKeys.end()) {
        return convertByType(key, it->second.type, jsonVal);
    }

    // 2. Handle dynamic options (e.g., option_Hash=128)
    if (key.starts_with("option_") || key.starts_with("option.")) {
         if (jsonVal.isString()) {
             return jsonVal.asString();
         }
         if (jsonVal.isBool()) {
             return jsonVal.asBool() ? std::string("true") : std::string("false");
         }
         if (jsonVal.isNumber()) {
             double d = jsonVal.asDouble();
             if (std::floor(d) == d) {
                 return std::to_string(static_cast<long long>(d));
             }
             return std::to_string(d);
         }
         throw AppError::makeInvalidParameters(std::format("Parameter '{}' has invalid value type.", key));
    }
    
    // 3. Unknown key
    throw AppError::makeInvalidParameters(std::format("Unknown engine parameter: '{}'. Please check the parameter name.", key));
}

std::string validateAndToString(const std::string& key, const JsonValue& jsonVal) {
    auto& manager = Settings::Manager::instance();
    
    // Check global definitions first
    const auto& globalDefs = manager.getDefinitions();
    if (auto it = globalDefs.find(key); it != globalDefs.end()) {
        auto val = convertByType(key, it->second.type, jsonVal);
        return valueToString(val);
    }

    // Check groups
    if (const size_t underscorePos = key.find('_'); underscorePos != std::string::npos) {
        // e.g. "sprt_alpha" -> group "sprt", key "alpha"
        const std::string groupName = key.substr(0, underscorePos);
        const std::string paramName = key.substr(underscorePos + 1);
        
        const auto& groupDefs = manager.getGroupDefinitions();
        if (auto gIt = groupDefs.find(groupName); gIt != groupDefs.end()) {
            if (auto pIt = gIt->second.keys.find(paramName); pIt != gIt->second.keys.end()) {
                auto val = convertByType(key, pIt->second.type, jsonVal);
                return valueToString(val);
            }
        }
    }
    
    // Fallback for non-strict or unknown params (like tool specific args)
    if (jsonVal.isString()) {
        return jsonVal.asString();
    }
    if (jsonVal.isBool()) {
        return jsonVal.asBool() ? "true" : "false";
    }
    if (jsonVal.isNumber()) {
        double d = jsonVal.asDouble();
        if (std::floor(d) == d) {
            return std::to_string(static_cast<long long>(d));
        }
        return std::to_string(d);
    }

    // If it's something else
    throw AppError::makeInvalidParameters(std::format("Parameter '{}' is unknown or has invalid type.", key));
}

} // namespace QaplaTester::Mcp
