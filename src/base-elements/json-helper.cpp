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

#include "json-helper.h"
#include <format>
#include <charconv>
#include <cctype>

namespace QaplaTester::Mcp {

JsonValue JsonHelper::makeNull() {
    return {};
}

JsonValue JsonHelper::makeBool(bool value) {
    return { .data = value };
}

JsonValue JsonHelper::makeNumber(double value) {
    return { .data = value };
}

JsonValue JsonHelper::makeString(std::string_view value) {
    return { .data = std::string(value) };
}

JsonValue JsonHelper::makeArray(JsonValue::Array value) {
    return { .data = std::move(value) };
}

JsonValue JsonHelper::makeObject(JsonValue::Object value) {
    return { .data = std::move(value) };
}

/**
 * @brief Visitor for serializing JsonValue data types.
 */
struct JsonSerializationVisitor {
    [[nodiscard]] std::string operator()(std::monostate /*unused*/) const {
        return "null";
    }

    [[nodiscard]] std::string operator()(bool argument) const {
        return argument ? "true" : "false";
    }

    [[nodiscard]] std::string operator()(double argument) const {
        return std::format("{}", argument);
    }

    [[nodiscard]] std::string operator()(const std::string& argument) const {
        std::string serializedString = "\"";
        for (const char character : argument) {
            if (character == '"') {
                serializedString += "\\\"";
            } else if (character == '\\') {
                serializedString += "\\\\";
            } else if (character == '\n') {
                serializedString += "\\n";
            } else {
                serializedString += character;
            }
        }
        return serializedString + "\"";
    }

    [[nodiscard]] std::string operator()(const JsonValue::Array& argument) const {
        std::string serializedArray = "[";
        for (size_t index = 0; index < argument.size(); ++index) {
            serializedArray += JsonHelper::serialize(argument[index]);
            if (index + 1 < argument.size()) {
                serializedArray += ",";
            }
        }
        return serializedArray + "]";
    }

    [[nodiscard]] std::string operator()(const JsonValue::Object& argument) const {
        std::string serializedObject = "{";
        size_t entryIndex = 0;
        for (const auto& [propertyName, propertyValue] : argument) {
            serializedObject += std::format("\"{}\":{}", propertyName, JsonHelper::serialize(propertyValue));
            if (++entryIndex < argument.size()) {
                serializedObject += ",";
            }
        }
        return serializedObject + "}";
    }
};

void JsonHelper::skipWhitespace(std::string_view& jsonText) {
    while (!jsonText.empty() && std::isspace(static_cast<unsigned char>(jsonText[0])) != 0) {
        jsonText.remove_prefix(1);
    }
}

JsonValue JsonHelper::parse(std::string_view& jsonText) {
    if (jsonText.empty()) {
        return {};
    }
    skipWhitespace(jsonText);
    if (jsonText.empty()) {
        return {};
    }

    const char firstChar = jsonText[0];
    if (firstChar == '{') {
        return parseObject(jsonText);
    }
    if (firstChar == '[') {
        return parseArray(jsonText);
    }
    if (firstChar == '"') {
        return parseString(jsonText);
    }
    if ((std::isdigit(static_cast<unsigned char>(firstChar)) != 0) || (firstChar == '-')) {
        return parseNumber(jsonText);
    }
    if ((firstChar == 't') || (firstChar == 'f') || (firstChar == 'n')) {
        return parseConstant(jsonText);
    }

    throw std::runtime_error(std::format("Unexpected character '{}' in JSON", firstChar));
}

JsonValue JsonHelper::parseObject(std::string_view& jsonText) {
    if (jsonText.empty() || (jsonText[0] != '{')) {
        return {};
    }
    jsonText.remove_prefix(1); // skip '{'
    JsonValue::Object resultObject;
    while (!jsonText.empty()) {
        skipWhitespace(jsonText);
        if (jsonText.empty()) {
            throw std::runtime_error("Unexpected end of JSON in object");
        }
        if (jsonText[0] == '}') {
            jsonText.remove_prefix(1);
            break;
        }

        JsonValue keyVal = parseString(jsonText);
        if (!keyVal.isString()) {
            throw std::runtime_error("Expected string key in object");
        }
        const auto& propertyKey = keyVal.asString();

        skipWhitespace(jsonText);
        if (!jsonText.empty() && (jsonText[0] == ':')) {
            jsonText.remove_prefix(1);
        } else {
            throw std::runtime_error("Expected ':' after key in object");
        }

        resultObject[propertyKey] = parse(jsonText);

        skipWhitespace(jsonText);
        if (!jsonText.empty() && (jsonText[0] == ',')) {
            jsonText.remove_prefix(1);
        } else if (!jsonText.empty() && (jsonText[0] == '}')) {
            jsonText.remove_prefix(1);
            break;
        } else if (jsonText.empty()) {
            throw std::runtime_error("Unexpected end of JSON in object after value");
        } else {
            throw std::runtime_error(std::format("Expected ',' or '}}' in object, found '{}'", jsonText[0]));
        }
    }
    return { .data = resultObject };
}

JsonValue JsonHelper::parseArray(std::string_view& jsonText) {
    if (jsonText.empty() || (jsonText[0] != '[')) {
        return {};
    }
    jsonText.remove_prefix(1); // skip '['
    JsonValue::Array resultArray;
    while (!jsonText.empty()) {
        skipWhitespace(jsonText);
        if (jsonText.empty()) {
            throw std::runtime_error("Unexpected end of JSON in array");
        }
        if (jsonText[0] == ']') {
            jsonText.remove_prefix(1);
            break;
        }

        resultArray.push_back(parse(jsonText));

        skipWhitespace(jsonText);
        if (!jsonText.empty() && (jsonText[0] == ',')) {
            jsonText.remove_prefix(1);
        } else if (!jsonText.empty() && (jsonText[0] == ']')) {
            jsonText.remove_prefix(1);
            break;
        } else if (jsonText.empty()) {
            throw std::runtime_error("Unexpected end of JSON in array after value");
        } else {
            throw std::runtime_error(std::format("Expected ',' or ']' in array, found '{}'", jsonText[0]));
        }
    }
    return { .data = resultArray };
}

JsonValue JsonHelper::parseString(std::string_view& jsonText) {
    if (jsonText.empty() || jsonText[0] != '"') {
        return {};
    }
    jsonText.remove_prefix(1);
    std::string resultString;
    while (!jsonText.empty() && jsonText[0] != '"') {
        if (jsonText[0] == '\\' && jsonText.size() > 1) {
            jsonText.remove_prefix(1);
            const char escapedChar = jsonText[0];
            if (escapedChar == 'n') {
                resultString += '\n';
            } else if (escapedChar == 't') {
                resultString += '\t';
            } else {
                resultString += escapedChar;
            }
        } else {
            resultString += jsonText[0];
        }
        jsonText.remove_prefix(1);
    }
    if (!jsonText.empty()) {
        jsonText.remove_prefix(1);
    }
    return { .data = resultString };
}

JsonValue JsonHelper::parseNumber(std::string_view& jsonText) {
    double numberValue = 0.0;
    const auto [ptr, errorCode] = std::from_chars(jsonText.data(), jsonText.data() + jsonText.size(), numberValue);
    if (errorCode == std::errc()) {
        jsonText.remove_prefix(ptr - jsonText.data());
    } else {
        throw std::runtime_error("Invalid number in JSON");
    }
    return { .data = numberValue };
}

JsonValue JsonHelper::parseConstant(std::string_view& jsonText) {
    if (jsonText.starts_with("true")) {
        jsonText.remove_prefix(4);
        return { .data = true };
    }
    if (jsonText.starts_with("false")) {
        jsonText.remove_prefix(5);
        return { .data = false };
    }
    if (jsonText.starts_with("null")) {
        jsonText.remove_prefix(4);
        return {};
    }
    throw std::runtime_error(std::format("Invalid constant in JSON, found '{}'", jsonText.substr(0, std::min(size_t(5), jsonText.size()))));
}

std::string JsonHelper::serialize(const JsonValue& value) {
    return std::visit(JsonSerializationVisitor{}, value.data);
}

} // namespace QaplaTester::Mcp
