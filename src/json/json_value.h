#pragma once

/**
 * @file json_value.h
 * @brief JavaScript-like JSON value API with parse and stringify support.
 */

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mqtt::json {

/**
 * @brief Dynamic JSON value that can store any JSON type.
 */
class JsonValue final {
public:
    /**
     * @brief JSON object type.
     */
    using Object = std::map<std::string, JsonValue>;

    /**
     * @brief JSON array type.
     */
    using Array = std::vector<JsonValue>;

    /**
     * @brief Creates a JSON null value.
     */
    JsonValue() = default;

    /**
     * @brief Creates a JSON boolean value.
     * @param booleanValue Value to store.
     */
    JsonValue(bool booleanValue);

    /**
     * @brief Creates a JSON number value.
     * @param numberValue Value to store.
     */
    JsonValue(double numberValue);

    /**
     * @brief Creates a JSON string value.
     * @param stringValue Value to store.
     */
    JsonValue(std::string stringValue);

    /**
     * @brief Creates a JSON string value.
     * @param stringValue Value to store.
     */
    JsonValue(const char* stringValue);

    /**
     * @brief Creates a JSON object value.
     * @param objectValue Value to store.
     */
    JsonValue(Object objectValue);

    /**
     * @brief Creates a JSON array value.
     * @param arrayValue Value to store.
     */
    JsonValue(Array arrayValue);

    /**
     * @brief Creates an empty JSON object.
     * @return New object value.
     */
    [[nodiscard]] static JsonValue object();

    /**
     * @brief Creates an empty JSON array.
     * @return New array value.
     */
    [[nodiscard]] static JsonValue array();

    /**
     * @brief Parses JSON text into a JsonValue.
     * @param jsonText Input JSON text.
     * @return Parsed JSON value.
     * @throws JsonException If parsing fails.
     */
    [[nodiscard]] static JsonValue parse(std::string_view jsonText);

    /**
     * @brief Parses JSON text into a JsonValue without exceptions.
     * @param jsonText Input JSON text.
     * @return Parsed JSON value on success, std::nullopt on failure.
     */
    [[nodiscard]] static std::optional<JsonValue> try_parse(std::string_view jsonText) noexcept;

    /**
     * @brief Serializes value to compact JSON text.
     * @return JSON text.
     * @throws JsonException If value cannot be serialized.
     */
    [[nodiscard]] std::string stringify() const;

    /**
     * @brief Serializes value using the legacy implementation.
     * @return JSON text.
     * @throws JsonException If value cannot be serialized.
     */
    [[nodiscard]] std::string stringify_legacy() const;

    /**
     * @brief Reports whether value is JSON null.
     * @return True when null.
     */
    [[nodiscard]] bool is_null() const noexcept;

    /**
     * @brief Reports whether value is JSON boolean.
     * @return True when boolean.
     */
    [[nodiscard]] bool is_boolean() const noexcept;

    /**
     * @brief Reports whether value is JSON number.
     * @return True when number.
     */
    [[nodiscard]] bool is_number() const noexcept;

    /**
     * @brief Reports whether value is JSON string.
     * @return True when string.
     */
    [[nodiscard]] bool is_string() const noexcept;

    /**
     * @brief Reports whether value is JSON object.
     * @return True when object.
     */
    [[nodiscard]] bool is_object() const noexcept;

    /**
     * @brief Reports whether value is JSON array.
     * @return True when array.
     */
    [[nodiscard]] bool is_array() const noexcept;

    /**
     * @brief Returns the contained boolean.
     * @return Boolean value.
     * @throws JsonException If value is not boolean.
     */
    [[nodiscard]] bool as_boolean() const;

    /**
     * @brief Returns the contained number.
     * @return Number value.
     * @throws JsonException If value is not number.
     */
    [[nodiscard]] double as_number() const;

    /**
     * @brief Returns the contained string.
     * @return String value.
     * @throws JsonException If value is not string.
     */
    [[nodiscard]] const std::string& as_string() const;

    /**
     * @brief Returns the contained object.
     * @return Object value.
     * @throws JsonException If value is not object.
     */
    [[nodiscard]] const Object& as_object() const;

    /**
     * @brief Returns the contained object.
     * @return Mutable object value.
     * @throws JsonException If value is not object.
     */
    [[nodiscard]] Object& as_object();

    /**
     * @brief Returns the contained array.
     * @return Array value.
     * @throws JsonException If value is not array.
     */
    [[nodiscard]] const Array& as_array() const;

    /**
     * @brief Returns the contained array.
     * @return Mutable array value.
     * @throws JsonException If value is not array.
     */
    [[nodiscard]] Array& as_array();

    /**
     * @brief Checks whether object contains a key.
     * @param keyName Key to check.
     * @return True when object has key.
     */
    [[nodiscard]] bool contains(std::string_view keyName) const;

    /**
     * @brief Returns object member by key.
     * @param keyName Key to access.
     * @return Member value.
     * @throws JsonException If value is not object or key is missing.
     */
    [[nodiscard]] const JsonValue& at(std::string_view keyName) const;

    /**
     * @brief Returns object member by key.
     * @param keyName Key to access.
     * @return Mutable member value.
     * @throws JsonException If value is not object or key is missing.
     */
    [[nodiscard]] JsonValue& at(std::string_view keyName);

    /**
     * @brief Returns array item by index.
     * @param indexValue Zero-based index.
     * @return Array item.
     * @throws JsonException If value is not array or index is out of range.
     */
    [[nodiscard]] const JsonValue& at(std::size_t indexValue) const;

    /**
     * @brief Returns array item by index.
     * @param indexValue Zero-based index.
     * @return Mutable array item.
     * @throws JsonException If value is not array or index is out of range.
     */
    [[nodiscard]] JsonValue& at(std::size_t indexValue);

    /**
     * @brief JavaScript-like object key access with auto-creation.
     * @param keyName Key to access.
     * @return Mutable member reference.
     * @throws JsonException If value is neither null nor object.
     */
    JsonValue& operator[](std::string_view keyName);

    /**
     * @brief JavaScript-like array index access with auto-growth.
     * @param indexValue Zero-based index.
     * @return Mutable item reference.
     * @throws JsonException If value is neither null nor array.
     */
    JsonValue& operator[](std::size_t indexValue);

    /**
     * @brief Appends an element to array.
     * @param elementValue Element to append.
     * @throws JsonException If value is neither null nor array.
     */
    void push_back(JsonValue elementValue);

    /**
     * @brief Returns size for object or array values.
     * @return Member or element count; zero for other types.
     */
    [[nodiscard]] std::size_t size() const noexcept;

private:
    using Storage = std::variant<std::monostate, bool, double, std::string, Object, Array>;

    Storage value_{};
};

} // namespace mqtt::json
