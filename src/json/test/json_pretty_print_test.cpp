#include <catch2/catch_test_macros.hpp>

#include "json/json_pretty_print.h"
#include "json/json_value.h"

#include <string>

using mqtt::json::JsonValue;
using mqtt::json::stringify_pretty;

namespace {

constexpr double k_expected_count{2.0};

} // namespace

TEST_CASE("stringify_pretty formats an empty object and array compactly", "[json][broker]") {
    CHECK(stringify_pretty(JsonValue::object()) == "{}");
    CHECK(stringify_pretty(JsonValue::array()) == "[]");
}

TEST_CASE("stringify_pretty formats a leaf value exactly like stringify", "[json][broker]") {
    CHECK(stringify_pretty(JsonValue{"hello\nworld"}) == JsonValue{"hello\nworld"}.stringify());
    CHECK(stringify_pretty(JsonValue{true}) == "true");
    CHECK(stringify_pretty(JsonValue{}) == "null");
}

TEST_CASE("stringify_pretty indents a nested object with one member per line", "[json][broker]") {
    JsonValue objectValue = JsonValue::object();
    objectValue["count"] = JsonValue{k_expected_count};
    objectValue["name"] = JsonValue{"qapla"};

    const std::string expectedText =
        "{\n"
        "  \"count\": 2,\n"
        "  \"name\": \"qapla\"\n"
        "}";
    CHECK(stringify_pretty(objectValue) == expectedText);
}

TEST_CASE("stringify_pretty indents nested arrays and objects at increasing depth", "[json][broker]") {
    JsonValue rootValue = JsonValue::object();
    JsonValue itemsValue = JsonValue::array();
    JsonValue firstItem = JsonValue::object();
    firstItem["id"] = JsonValue{1.0};
    itemsValue.push_back(firstItem);
    rootValue["items"] = itemsValue;

    const std::string expectedText =
        "{\n"
        "  \"items\": [\n"
        "    {\n"
        "      \"id\": 1\n"
        "    }\n"
        "  ]\n"
        "}";
    CHECK(stringify_pretty(rootValue) == expectedText);
}

TEST_CASE("stringify_pretty honors a custom indent width", "[json][broker]") {
    JsonValue objectValue = JsonValue::object();
    objectValue["a"] = JsonValue{1.0};

    const std::string expectedText =
        "{\n"
        "    \"a\": 1\n"
        "}";
    CHECK(stringify_pretty(objectValue, 4U) == expectedText);
}

TEST_CASE("stringify_pretty round-trips to the same value as stringify", "[json][broker]") {
    const std::string inputText{R"({"a":[1,2,3],"b":{"c":true},"d":null,"e":"x\ny"})"};
    const JsonValue parsedValue = JsonValue::parse(inputText);

    const JsonValue reparsedValue = JsonValue::parse(stringify_pretty(parsedValue));
    CHECK(reparsedValue.stringify() == parsedValue.stringify());
}
