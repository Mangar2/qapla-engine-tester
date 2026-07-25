# src/json — JSON Value Module

Reusable JSON value module with JavaScript-like access semantics for YAHA and broker-side components.

## Purpose

Provide a single in-repo JSON abstraction with:
- `parse` from JSON text to a dynamic value object.
- `stringify` from dynamic value object to compact JSON text.
- object and array access patterns close to JavaScript (`operator[]` auto-creation and auto-growth on mutable access).

This module is transport- and feature-agnostic and can be reused by automation, message store, and other YAHA clients.

## Public API

### `json/json_error.h`

- `enum class JsonError : unsigned char`
- `class JsonException`

Error categories cover parser syntax, invalid escapes, invalid type access, missing object keys, and array index bounds.

### `json/json_value.h`

`class JsonValue` with the following behavior:

- Dynamic storage types: `null`, `bool`, `double`, `string`, `Object`, `Array`.
- Object type: `std::map<std::string, JsonValue>`.
- Array type: `std::vector<JsonValue>`.

Construction:
- default constructor creates `null`.
- constructors for `bool`, `double`, `string`, object, and array.
- helpers `JsonValue::object()` and `JsonValue::array()`.

Parsing and serialization:
- `static JsonValue parse(std::string_view jsonText)` throws `JsonException` on malformed input.
- `static std::optional<JsonValue> try_parse(std::string_view jsonText) noexcept` returns `std::nullopt` on parse failure.
- `std::string stringify() const` serializes to compact JSON using a two-phase path: first computes exact output size for the whole tree, then writes left-to-right into one pre-sized string buffer.
- `std::string stringify_legacy() const` keeps the previous append-based serializer for regression comparison tests.

Type inspection and access:
- `is_null`, `is_boolean`, `is_number`, `is_string`, `is_object`, `is_array`.
- strict typed getters `as_*` that throw `JsonException(JsonError::InvalidType)` when incompatible.

JavaScript-like mutable access:
- `operator[](std::string_view keyName)`:
  - converts `null` to `{}`.
  - creates missing keys as `null` entries.
  - throws if current value is neither `null` nor object.
- `operator[](std::size_t indexValue)`:
  - converts `null` to `[]`.
  - auto-grows array with `null` fillers up to index.
  - throws if current value is neither `null` nor array.
- `push_back(JsonValue)` converts `null` to `[]` then appends.

Strict lookup API:
- `at(key)` throws `JsonError::MissingKey` for absent keys.
- `at(index)` throws `JsonError::IndexOutOfRange` for invalid index.

## Parser details

`json/json_value.cpp` contains a recursive-descent parser supporting:
- JSON objects/arrays.
- `true`, `false`, `null`.
- RFC-style number syntax with finite-number enforcement.
- string escapes (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX`).
- UTF-16 surrogate pair handling for `\uD800..\uDBFF` + `\uDC00..\uDFFF`.

On parse failure, errors include source offset in `JsonException::offset()`.

## Stringify details

`stringify()` avoids slow streaming APIs and unnecessary dynamic growth during serialization:
- no `std::ostringstream` in the fast path.
- exact size pre-calculation for full JSON output before allocation.
- single target allocation and sequential write cursor fill.

`stringify_legacy()` preserves previous behavior and formatting to support output parity checks in tests.
