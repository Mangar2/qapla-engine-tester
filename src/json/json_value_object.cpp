#include "json/json_value.h"
#include "json/json_value_object.h"

#include "json/json_error.h"

#include <format>
#include <string>

namespace mqtt::json::detail {

[[noreturn]] void throw_invalid_object_type() {
    throw JsonException(JsonError::InvalidType, "value is not an object", 0U);
}

[[noreturn]] void throw_missing_key(const std::string_view keyName) {
    throw JsonException(JsonError::MissingKey,
                        std::format("missing key '{}'", keyName),
                        0U);
}

} // namespace mqtt::json::detail

namespace mqtt::json {

const JsonValue::Object& JsonValue::as_object() const {
    if (!is_object()) {
        detail::throw_invalid_object_type();
    }
    return std::get<Object>(value_);
}

JsonValue::Object& JsonValue::as_object() {
    if (!is_object()) {
        detail::throw_invalid_object_type();
    }
    return std::get<Object>(value_);
}

bool JsonValue::contains(const std::string_view keyName) const {
    if (!is_object()) {
        return false;
    }

    const auto& objectValue = std::get<Object>(value_);
    return objectValue.contains(std::string{keyName});
}

const JsonValue& JsonValue::at(const std::string_view keyName) const {
    if (!is_object()) {
        detail::throw_invalid_object_type();
    }

    const auto& objectValue = std::get<Object>(value_);
    const auto iterator = objectValue.find(std::string{keyName});
    if (iterator == objectValue.end()) {
        detail::throw_missing_key(keyName);
    }
    return iterator->second;
}

JsonValue& JsonValue::at(const std::string_view keyName) {
    if (!is_object()) {
        detail::throw_invalid_object_type();
    }

    auto& objectValue = std::get<Object>(value_);
    const auto iterator = objectValue.find(std::string{keyName});
    if (iterator == objectValue.end()) {
        detail::throw_missing_key(keyName);
    }
    return iterator->second;
}

JsonValue& JsonValue::operator[](const std::string_view keyName) {
    if (is_null()) {
        value_ = Object{};
    }
    if (!is_object()) {
        throw JsonException(JsonError::InvalidType,
                            "key access requires object or null value",
                            0U);
    }

    auto& objectValue = std::get<Object>(value_);
    return objectValue[std::string{keyName}];
}

} // namespace mqtt::json
