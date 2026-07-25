#include "json/json_value.h"
#include "json/json_value_array.h"

#include "json/json_error.h"

#include <format>
#include <utility>

namespace mqtt::json::detail {

[[noreturn]] void throw_invalid_array_type() {
    throw JsonException(JsonError::InvalidType, "value is not an array", 0U);
}

[[noreturn]] void throw_index_out_of_range(const std::size_t indexValue) {
    throw JsonException(JsonError::IndexOutOfRange,
                        std::format("index {} out of range", indexValue),
                        0U);
}

} // namespace mqtt::json::detail

namespace mqtt::json {

const JsonValue::Array& JsonValue::as_array() const {
    if (!is_array()) {
        detail::throw_invalid_array_type();
    }
    return std::get<Array>(value_);
}

JsonValue::Array& JsonValue::as_array() {
    if (!is_array()) {
        detail::throw_invalid_array_type();
    }
    return std::get<Array>(value_);
}

const JsonValue& JsonValue::at(const std::size_t indexValue) const {
    if (!is_array()) {
        detail::throw_invalid_array_type();
    }

    const auto& arrayValue = std::get<Array>(value_);
    if (indexValue >= arrayValue.size()) {
        detail::throw_index_out_of_range(indexValue);
    }
    return arrayValue[indexValue];
}

JsonValue& JsonValue::at(const std::size_t indexValue) {
    if (!is_array()) {
        detail::throw_invalid_array_type();
    }

    auto& arrayValue = std::get<Array>(value_);
    if (indexValue >= arrayValue.size()) {
        detail::throw_index_out_of_range(indexValue);
    }
    return arrayValue[indexValue];
}

JsonValue& JsonValue::operator[](const std::size_t indexValue) {
    if (is_null()) {
        value_ = Array{};
    }
    if (!is_array()) {
        throw JsonException(JsonError::InvalidType,
                            "index access requires array or null value",
                            0U);
    }

    auto& arrayValue = std::get<Array>(value_);
    if (indexValue >= arrayValue.size()) {
        arrayValue.resize(indexValue + 1U);
    }
    return arrayValue[indexValue];
}

void JsonValue::push_back(JsonValue elementValue) {
    if (is_null()) {
        value_ = Array{};
    }
    if (!is_array()) {
        throw JsonException(JsonError::InvalidType,
                            "push_back requires array or null value",
                            0U);
    }

    auto& arrayValue = std::get<Array>(value_);
    arrayValue.push_back(std::move(elementValue));
}

} // namespace mqtt::json
