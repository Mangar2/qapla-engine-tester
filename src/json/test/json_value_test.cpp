#include <catch2/catch_test_macros.hpp>

#include "json/json_error.h"
#include "json/json_value.h"

#include <limits>
#include <optional>
#include <functional>
#include <string>

using mqtt::json::JsonError;
using mqtt::json::JsonException;
using mqtt::json::JsonValue;

namespace {

constexpr double k_expected_list_value{3.0};
constexpr double k_expected_array_value{42.0};
constexpr double k_expected_exponent_number{-1250.0};
constexpr double k_push_back_element_number{2.0};
constexpr double k_expected_positive_exponent_number{100.0};
constexpr double k_object_item_number{7.0};
constexpr double k_regression_number_value{-1250.25};
constexpr unsigned char k_control_char_etx{0x03U};
constexpr unsigned char k_control_char_us{0x1FU};

void expect_json_error(const std::function<void()>& operation,
                       JsonError expectedError) {
    try {
        operation();
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == expectedError);
    }
}

} // namespace

TEST_CASE("parse_object_fields_are_accessible", "[json][broker]") {
    const std::string inputText{R"({"name":"yaha","active":true,"list":[1,2,3],"text":"line\nnext"})"};

    const JsonValue parsedValue = JsonValue::parse(inputText);

    REQUIRE(parsedValue.is_object());
    CHECK(parsedValue.at("name").as_string() == "yaha");
    CHECK(parsedValue.at("active").as_boolean());
    CHECK(parsedValue.at("list").at(2U).as_number() == k_expected_list_value);
    CHECK(parsedValue.at("text").as_string() == "line\nnext");
}

TEST_CASE("parse_object_members_with_whitespace_after_comma", "[json][broker]") {
    const std::string inputText{
        "{\n"
        "  \"first\": 1,\n"
        "  \"second\": true,\n"
        "  \"third\": \"ok\"\n"
        "}"};

    const JsonValue parsedValue = JsonValue::parse(inputText);

    REQUIRE(parsedValue.is_object());
    CHECK(parsedValue.at("first").as_number() == 1.0);
    CHECK(parsedValue.at("second").as_boolean());
    CHECK(parsedValue.at("third").as_string() == "ok");
}

TEST_CASE("stringify_and_parse_roundtrip_keeps_values", "[json][broker]") {
    const JsonValue parsedValue = JsonValue::parse(
        R"({"name":"yaha","active":true,"list":[1,2,3],"text":"line\nnext"})");

    const std::string serializedText = parsedValue.stringify();
    const JsonValue reparsedValue = JsonValue::parse(serializedText);

    CHECK(reparsedValue.at("name").as_string() == "yaha");
    CHECK(reparsedValue.at("list").at(0U).as_number() == 1.0);
}

TEST_CASE("parse_invalid_json_returns_nullopt", "[json][broker]") {
    const std::optional<JsonValue> parsedValue = JsonValue::try_parse("{ \"a\": [1, 2 }");
    REQUIRE_FALSE(parsedValue.has_value());
}

TEST_CASE("parse_invalid_json_throws_with_offset", "[json][broker]") {
    try {
        (void)JsonValue::parse("{ \"a\" 1 }");
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::UnexpectedToken);
        CHECK(exception.offset() > 0U);
    }
}

TEST_CASE("object_operator_brackets_auto_create", "[json][broker]") {
    JsonValue rootValue{};
    rootValue["rules"]["entry"]["enabled"] = JsonValue{true};
    rootValue["rules"]["entry"]["name"] = JsonValue{"main"};

    REQUIRE(rootValue.is_object());
    CHECK(rootValue.at("rules").at("entry").at("enabled").as_boolean());
    CHECK(rootValue.at("rules").at("entry").at("name").as_string() == "main");
}

TEST_CASE("array_operator_brackets_auto_growth", "[json][broker]") {
    JsonValue arrayValue{};
    arrayValue[2U] = JsonValue{k_expected_array_value};

    REQUIRE(arrayValue.is_array());
    CHECK(arrayValue.size() == 3U);
    CHECK(arrayValue.at(0U).is_null());
    CHECK(arrayValue.at(1U).is_null());
    CHECK(arrayValue.at(2U).as_number() == k_expected_array_value);
}

TEST_CASE("push_back_on_null_creates_array", "[json][broker]") {
    JsonValue arrayValue{};
    arrayValue.push_back(JsonValue{"a"});
    arrayValue.push_back(JsonValue{"b"});

    REQUIRE(arrayValue.is_array());
    CHECK(arrayValue.size() == 2U);
    CHECK(arrayValue.at(0U).as_string() == "a");
    CHECK(arrayValue.at(1U).as_string() == "b");
}

TEST_CASE("invalid_type_access_throws", "[json][broker]") {
    const JsonValue numberValue{5.0};
    try {
        (void)numberValue.as_string();
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidType);
    }
}

TEST_CASE("missing_key_throws", "[json][broker]") {
    JsonValue objectValue = JsonValue::object();
    objectValue["present"] = JsonValue{1.0};

    try {
        (void)objectValue.at("missing");
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::MissingKey);
    }
}

TEST_CASE("out_of_range_index_throws", "[json][broker]") {
    JsonValue arrayValue = JsonValue::array();
    arrayValue.push_back(JsonValue{1.0});

    try {
        (void)arrayValue.at(3U);
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::IndexOutOfRange);
    }
}

TEST_CASE("unicode_escape_parsing_supports_surrogate_pair", "[json][broker]") {
    const JsonValue parsedValue = JsonValue::parse(R"("\uD83D\uDE03")");

    REQUIRE(parsedValue.is_string());
    CHECK_FALSE(parsedValue.as_string().empty());
}

TEST_CASE("parse_scalar_literals_and_exponent_number", "[json][broker]") {
    const JsonValue nullValue = JsonValue::parse("null");
    const JsonValue trueValue = JsonValue::parse("true");
    const JsonValue falseValue = JsonValue::parse("false");
    const JsonValue exponentNumber = JsonValue::parse("-12.5e2");

    CHECK(nullValue.is_null());
    CHECK(trueValue.as_boolean());
    CHECK_FALSE(falseValue.as_boolean());
    CHECK(exponentNumber.as_number() == k_expected_exponent_number);
}

TEST_CASE("parse_rejects_leading_zero_number", "[json][broker]") {
    try {
        (void)JsonValue::parse("01");
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidNumber);
    }
}

TEST_CASE("parse_rejects_invalid_string_escape", "[json][broker]") {
    try {
        (void)JsonValue::parse(R"("\x")");
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidStringEscape);
    }
}

TEST_CASE("parse_rejects_invalid_unicode_surrogate_sequence", "[json][broker]") {
    try {
        (void)JsonValue::parse(R"("\uDE03")");
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidUnicodeEscape);
    }
}

TEST_CASE("stringify_non_finite_number_throws", "[json][broker]") {
    const JsonValue nonFiniteValue{std::numeric_limits<double>::infinity()};
    try {
        (void)nonFiniteValue.stringify();
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidNumber);
    }
}

TEST_CASE("operators_on_incompatible_types_throw", "[json][broker]") {
    JsonValue boolValue{true};
    JsonValue textValue{"x"};
    JsonValue numberValue{1.0};

    try {
        (void)boolValue["child"];
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidType);
    }

    try {
        (void)textValue[0U];
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidType);
    }

    try {
        numberValue.push_back(JsonValue{k_push_back_element_number});
        FAIL("expected JsonException");
    } catch (const JsonException& exception) {
        CHECK(exception.error() == JsonError::InvalidType);
    }
}

TEST_CASE("parse_and_stringify_escape_matrix", "[json][broker]") {
    const JsonValue parsedValue = JsonValue::parse(
        R"("\"\\\/\b\f\n\r\t")");

    REQUIRE(parsedValue.is_string());
    const std::string serializedText = parsedValue.stringify();
    CHECK(serializedText.find("\\\"") != std::string::npos);
    CHECK(serializedText.find("\\\\") != std::string::npos);
    CHECK(serializedText.find("\\n") != std::string::npos);
    CHECK(serializedText.find("\\r") != std::string::npos);
    CHECK(serializedText.find("\\t") != std::string::npos);
}

TEST_CASE("stringify_escapes_ascii_control_characters_as_unicode", "[json][broker]") {
    std::string textWithControl{"A"};
    textWithControl.push_back(static_cast<char>(k_control_char_etx));
    textWithControl.push_back(static_cast<char>(k_control_char_us));
    textWithControl += "Z";

    const JsonValue value{textWithControl};
    const std::string serializedText = value.stringify();
    CHECK(serializedText == "\"A\\u0003\\u001fZ\"");

    const JsonValue reparsedValue = JsonValue::parse(serializedText);
    CHECK(reparsedValue.as_string() == textWithControl);
}

TEST_CASE("stringify_new_matches_legacy_output", "[json][broker]") {
    JsonValue rootValue = JsonValue::object();
    rootValue["name"] = JsonValue{"yaha"};
    rootValue["active"] = JsonValue{true};
    rootValue["number"] = JsonValue{k_regression_number_value};

    std::string textBuffer{"line\nnext\t\"quoted\""};
    textBuffer.push_back(static_cast<char>(k_control_char_etx));
    JsonValue textValue{textBuffer};
    rootValue["text"] = textValue;

    JsonValue listValue = JsonValue::array();
    listValue.push_back(JsonValue{1.0});
    listValue.push_back(JsonValue{false});
    listValue.push_back(JsonValue{"x"});

    JsonValue nestedValue = JsonValue::object();
    nestedValue["emptyArray"] = JsonValue::array();
    nestedValue["emptyObject"] = JsonValue::object();
    nestedValue["list"] = listValue;
    rootValue["nested"] = nestedValue;

    const std::string fastSerializedText = rootValue.stringify();
    const std::string legacySerializedText = rootValue.stringify_legacy();

    CHECK(fastSerializedText == legacySerializedText);
}

TEST_CASE("parse_empty_object_and_array_and_trailing_token_error", "[json][broker]") {
    const JsonValue emptyObject = JsonValue::parse("{}");
    const JsonValue emptyArray = JsonValue::parse("[]");
    const JsonValue exponentValue = JsonValue::parse("1e+2");

    CHECK(emptyObject.is_object());
    CHECK(emptyArray.is_array());
    CHECK(exponentValue.as_number() == k_expected_positive_exponent_number);

    expect_json_error([]() {
        (void)JsonValue::parse("{}x");
    }, JsonError::UnexpectedToken);
    expect_json_error([]() {
        (void)JsonValue::parse("");
    }, JsonError::UnexpectedEndOfInput);
}

TEST_CASE("parse_unicode_paths_cover_utf8_widths", "[json][broker]") {
    const JsonValue oneByteValue = JsonValue::parse(R"("\u0041")");
    const JsonValue twoByteValue = JsonValue::parse(R"("\u00a9")");
    const JsonValue threeByteValue = JsonValue::parse(R"("\u20AC")");
    const JsonValue fourByteValue = JsonValue::parse(R"("\uD83D\uDE03")");

    CHECK(oneByteValue.is_string());
    CHECK(twoByteValue.is_string());
    CHECK(threeByteValue.is_string());
    CHECK(fourByteValue.is_string());

    expect_json_error([]() {
        (void)JsonValue::parse(R"("\uD800")");
    }, JsonError::InvalidUnicodeEscape);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("accessors_and_size_cover_const_and_mutable_paths", "[json][broker]") {
    JsonValue objectValue = JsonValue::object();
    objectValue["item"] = JsonValue{k_object_item_number};

    JsonValue arrayValue = JsonValue::array();
    arrayValue.push_back(JsonValue{1.0});

    JsonValue nullValue{};
    CHECK(nullValue.stringify() == "null");

    CHECK(objectValue.contains("item"));
    CHECK_FALSE(objectValue.contains("missing"));
    CHECK_FALSE(arrayValue.contains("item"));

    CHECK(objectValue.as_object().size() == 1U);
    CHECK(arrayValue.as_array().size() == 1U);
    CHECK(objectValue.size() == 1U);
    CHECK(nullValue.size() == 0U);
}

TEST_CASE("accessor_invalid_type_paths_throw", "[json][broker]") {
    JsonValue objectValue = JsonValue::object();
    objectValue["item"] = JsonValue{k_object_item_number};
    JsonValue arrayValue = JsonValue::array();
    arrayValue.push_back(JsonValue{1.0});

    const JsonValue& constObjectReference = objectValue;
    const JsonValue& constArrayReference = arrayValue;

    expect_json_error([&constObjectReference]() {
        (void)constObjectReference.at(0U);
    }, JsonError::InvalidType);
    expect_json_error([&constArrayReference]() {
        (void)constArrayReference.at("item");
    }, JsonError::InvalidType);
    expect_json_error([]() {
        (void)JsonValue{"x"}.as_boolean();
    }, JsonError::InvalidType);
    expect_json_error([]() {
        (void)JsonValue{true}.as_number();
    }, JsonError::InvalidType);
}
