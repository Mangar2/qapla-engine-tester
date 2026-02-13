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
#include <vector>
#include <map>
#include <variant>
#include <string_view>

namespace QaplaTester::Mcp {

/**
 * @brief Minimal JSON value representation for MCP.
 */
struct JsonValue {
    using Object = std::map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;
    std::variant<std::monostate, bool, double, std::string, Array, Object> data;

    [[nodiscard]] bool isObject() const { return std::holds_alternative<Object>(data); }
    [[nodiscard]] const Object& asObject() const { return std::get<Object>(data); }
    [[nodiscard]] bool isString() const { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] const std::string& asString() const { return std::get<std::string>(data); }
    [[nodiscard]] bool isNumber() const { return std::holds_alternative<double>(data); }
    [[nodiscard]] double asDouble() const { return std::get<double>(data); }
    [[nodiscard]] bool isBool() const { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool asBool() const { return std::get<bool>(data); }
    [[nodiscard]] bool isArray() const { return std::holds_alternative<Array>(data); }
    [[nodiscard]] const Array& asArray() const { return std::get<Array>(data); }
};

/**
 * @brief Utility class for simple JSON parsing and serialization.
 */
class JsonHelper {
public:
    /**
     * @brief Parses a JSON string into a JsonValue.
     * @param jsonText The string view to parse, will be advanced.
     * @return Parsed JSON value.
     */
    [[nodiscard]] static JsonValue parse(std::string_view& jsonText);

    /**
     * @brief Serializes a JsonValue to a string.
     * @param value The value to serialize.
     * @return JSON string representation.
     */
    [[nodiscard]] static std::string serialize(const JsonValue& value);

private:
    static void skipWhitespace(std::string_view& jsonText);
    [[nodiscard]] static JsonValue parseObject(std::string_view& jsonText);
    [[nodiscard]] static JsonValue parseArray(std::string_view& jsonText);
    [[nodiscard]] static JsonValue parseString(std::string_view& jsonText);
    [[nodiscard]] static JsonValue parseNumber(std::string_view& jsonText);
    [[nodiscard]] static JsonValue parseConstant(std::string_view& jsonText);
};

} // namespace QaplaTester::Mcp
