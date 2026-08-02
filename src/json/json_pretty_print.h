#pragma once

/**
 * @file json_pretty_print.h
 * @brief Indented, human-readable JSON serialization for JsonValue.
 */

#include "json_value.h"

#include <cstddef>
#include <string>

namespace mqtt::json {

/**
 * @brief Serializes value to indented, human-readable JSON text.
 *
 * Object members are written in the same order stringify() uses (Object is a std::map, so
 * alphabetical by key). Leaf values (string/number/boolean/null) are formatted via stringify()
 * itself, so escaping and number formatting stay identical to the compact serializer -- this
 * function only adds indentation/line breaks around objects and arrays.
 *
 * @param inputValue Value to serialize.
 * @param indentWidth Spaces per nesting level.
 * @return Pretty-printed JSON text.
 * @throws JsonException If value cannot be serialized.
 */
[[nodiscard]] std::string stringify_pretty(const JsonValue& inputValue, std::size_t indentWidth = 2U);

} // namespace mqtt::json
