/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "json-helper.h"
#include <format>
#include <charconv>
#include <cctype>

namespace QaplaTester::Mcp {

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
    if (std::isdigit(static_cast<unsigned char>(firstChar)) != 0 || firstChar == '-') {
        return parseNumber(jsonText);
    }
    return parseConstant(jsonText);
}

JsonValue JsonHelper::parseObject(std::string_view& jsonText) {
    jsonText.remove_prefix(1); // skip '{'
    JsonValue::Object resultObject;
    while (!jsonText.empty()) {
        skipWhitespace(jsonText);
        if (jsonText[0] == '}') {
            jsonText.remove_prefix(1);
            break;
        }

        JsonValue keyVal = parseString(jsonText);
        std::string propertyKey = keyVal.isString() ? keyVal.asString() : "";

        skipWhitespace(jsonText);
        if (!jsonText.empty() && jsonText[0] == ':') {
            jsonText.remove_prefix(1);
        }

        resultObject[propertyKey] = parse(jsonText);

        skipWhitespace(jsonText);
        if (!jsonText.empty() && jsonText[0] == ',') {
            jsonText.remove_prefix(1);
        } else if (!jsonText.empty() && jsonText[0] == '}') {
            jsonText.remove_prefix(1);
            break;
        }
    }
    return { .data = resultObject };
}

JsonValue JsonHelper::parseArray(std::string_view& jsonText) {
    jsonText.remove_prefix(1); // skip '['
    JsonValue::Array resultArray;
    while (!jsonText.empty()) {
        skipWhitespace(jsonText);
        if (jsonText[0] == ']') {
            jsonText.remove_prefix(1);
            break;
        }
        resultArray.push_back(parse(jsonText));
        skipWhitespace(jsonText);
        if (!jsonText.empty() && jsonText[0] == ',') {
            jsonText.remove_prefix(1);
        } else if (!jsonText.empty() && jsonText[0] == ']') {
            jsonText.remove_prefix(1);
            break;
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
    return {};
}

std::string JsonHelper::serialize(const JsonValue& value) {
    return std::visit(JsonSerializationVisitor{}, value.data);
}

} // namespace QaplaTester::Mcp
