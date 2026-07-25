# json/test — TEST_SPEC

## JsonValue::parse + stringify

1. parse_object_and_stringify_roundtrip
- Scenario: Parse nested object with array and escape sequence.
- Input: `{"name":"yaha","active":true,"list":[1,2,3],"text":"line\\nnext"}`
- Expected: parse succeeds; `stringify()` returns equivalent compact JSON; values are accessible by key/index.

2. parse_invalid_json_returns_nullopt
- Scenario: Parse malformed JSON text via `try_parse`.
- Input: `{ "a": [1, 2 }`
- Expected: returns `std::nullopt`.

3. parse_invalid_json_throws_with_offset
- Scenario: Parse malformed JSON text via throwing API.
- Input: `{ "a" 1 }`
- Expected: throws `JsonException` with `JsonError::UnexpectedToken` and non-zero/valid offset.

## JavaScript-like object and array behavior

4. object_operator_brackets_auto_create
- Scenario: Build object tree using key-based `operator[]`.
- Input: start with null value and assign nested keys.
- Expected: value becomes object; nested keys are created and values readable.

5. array_operator_brackets_auto_growth
- Scenario: Build array using index-based `operator[]` on null value.
- Input: assign index `2`.
- Expected: value becomes array of size `3`; missing entries are null.

6. push_back_on_null_creates_array
- Scenario: Append to null value.
- Input: `push_back("a")`, `push_back("b")`.
- Expected: value becomes array with two entries.

7. invalid_type_access_throws
- Scenario: Access number as string.
- Input: `JsonValue{5.0}.as_string()`
- Expected: throws `JsonException` with `JsonError::InvalidType`.

8. missing_key_throws
- Scenario: Access missing object key with `at`.
- Input: object without requested key.
- Expected: throws `JsonException` with `JsonError::MissingKey`.

9. out_of_range_index_throws
- Scenario: Access array index beyond bounds with `at`.
- Input: array with one entry and index `3`.
- Expected: throws `JsonException` with `JsonError::IndexOutOfRange`.

10. unicode_escape_parsing_supports_surrogate_pair
- Scenario: Parse string with UTF-16 surrogate pair escape.
- Input: `"\uD83D\uDE03"`
- Expected: parse succeeds and resulting UTF-8 string is non-empty.

11. parse_scalar_literals_and_exponent_number
- Scenario: Parse JSON literals and exponent number.
- Input: `null`, `true`, `false`, `-12.5e2`
- Expected: parsed values have expected type/value.

12. parse_rejects_leading_zero_number
- Scenario: Parser rejects invalid number with leading zero.
- Input: `01`
- Expected: `parse` throws `JsonException` with `JsonError::InvalidNumber`.

13. parse_rejects_invalid_string_escape
- Scenario: Parser rejects invalid escape in string.
- Input: `"\x"`
- Expected: `parse` throws `JsonException` with `JsonError::InvalidStringEscape`.

14. parse_rejects_invalid_unicode_surrogate_sequence
- Scenario: Parser rejects standalone low surrogate.
- Input: `"\uDE03"`
- Expected: `parse` throws `JsonException` with `JsonError::InvalidUnicodeEscape`.

15. stringify_non_finite_number_throws
- Scenario: Serializer rejects non-finite number.
- Input: `JsonValue{infinity}`
- Expected: `stringify` throws `JsonException` with `JsonError::InvalidNumber`.

16. operators_on_incompatible_types_throw
- Scenario: JS-like mutable operators used on incompatible scalar value.
- Input: `JsonValue{true}["key"]`, `JsonValue{"x"}[0]`, `JsonValue{1.0}.push_back(...)`
- Expected: each operation throws `JsonException` with `JsonError::InvalidType`.

17. parse_and_stringify_escape_matrix
- Scenario: Parser and serializer handle all standard JSON escapes.
- Input: string containing `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`
- Expected: parse succeeds and stringify emits escaped forms.

18. parse_empty_object_and_array_and_trailing_token_error
- Scenario: Cover empty container parsing and trailing-token rejection.
- Input: `{}`, `[]`, and `{}x`
- Expected: empty containers parse successfully; trailing token throws `JsonError::UnexpectedToken`.

19. parse_unicode_paths_cover_utf8_widths
- Scenario: Parse Unicode escapes mapping to 1/2/3/4-byte UTF-8 outputs.
- Input: `\u0041`, `\u00A9`, `\u20AC`, `\uD83D\uDE03`
- Expected: parse succeeds for all inputs.

20. accessors_and_size_cover_const_and_mutable_paths
- Scenario: Exercise `as_object/as_array` mutable overloads, `contains`, const `at` invalid-type throws, and size on object/null.
- Input: object/array/null values with lookups and invalid lookups.
- Expected: success and throw paths match expected `JsonError` values.

21. accessor_invalid_type_paths_throw
- Scenario: Exercise invalid-type throw paths for const `at(index)`, const `at(key)`, `as_boolean`, and `as_number`.
- Input: object/array plus mismatched accessor calls.
- Expected: each call throws `JsonException` with `JsonError::InvalidType`.

22. stringify_escapes_ascii_control_characters_as_unicode
- Scenario: Serializer must escape raw ASCII control bytes below `0x20` using Unicode escape form.
- Input: string value containing bytes `0x03` and `0x1F`.
- Expected: `stringify()` returns escaped JSON string with `\u0003` and `\u001f`, and parsing the result restores the original bytes.

23. stringify_new_matches_legacy_output
- Scenario: New pre-sized stringify must stay output-compatible with the previous implementation.
- Input: nested object with arrays, numbers, escapes, and control characters.
- Expected: `stringify()` output is identical to `stringify_legacy()` output.

24. parse_object_members_with_whitespace_after_comma
- Scenario: Object parser accepts valid whitespace/newline formatting before subsequent key names.
- Input: multi-line object with spaces/newlines after member separators.
- Expected: parse succeeds and all object fields are readable.
