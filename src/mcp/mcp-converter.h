/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <string>
#include "json-helper.h"
#include "../cli/settings-manager.h"

namespace QaplaTester::Mcp {

/**
 * @brief Converts a JSON value to a strongly-typed Settings::Value based on the engine configuration schema.
 * 
 * @param key The configuration key (parameter name).
 * @param jsonVal The JSON value to convert.
 * @return Settings::Value The converted value matching the schema definition.
 * 
 * @throws AppError If the key is not a valid engine parameter.
 * @throws AppError If the JSON value type does not match the schema type definition.
 */
Settings::Value convertJsonToEngineSetting(const std::string& key, const JsonValue& jsonVal);

/**
 * @brief Validates a JSON value against the global/group settings schema and converts it to a string.
 * This is used for populating ConfigData/IniFile structures where all values are stored as strings.
 * 
 * @param key The parameter key (e.g. "sprt_alpha", "log_level").
 * @param jsonVal The JSON value to map.
 * @return std::string The string representation of the value.
 * 
 * @throws AppError If types mismatch or validation fails.
 */
std::string validateAndToString(const std::string& key, const JsonValue& jsonVal);

} // namespace QaplaTester::Mcp
