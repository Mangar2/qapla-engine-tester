#pragma once

/**
 * @file json_value_object.h
 * @brief Object-related implementation partition for JsonValue.
 */

#include <string_view>

namespace mqtt::json::detail {

/**
 * @brief Throws InvalidType for object-only APIs.
 */
[[noreturn]] void throw_invalid_object_type();

/**
 * @brief Throws MissingKey for object key lookup failures.
 * @param keyName Missing key.
 */
[[noreturn]] void throw_missing_key(std::string_view keyName);

} // namespace mqtt::json::detail
