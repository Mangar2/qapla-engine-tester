#pragma once

/**
 * @file json_value_array.h
 * @brief Array-related implementation partition for JsonValue.
 */

#include <cstddef>

namespace mqtt::json::detail {

/**
 * @brief Throws InvalidType for array-only APIs.
 */
[[noreturn]] void throw_invalid_array_type();

/**
 * @brief Throws IndexOutOfRange for invalid array index access.
 * @param indexValue Requested array index.
 */
[[noreturn]] void throw_index_out_of_range(std::size_t indexValue);

} // namespace mqtt::json::detail
