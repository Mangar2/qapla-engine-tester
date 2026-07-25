#include <catch2/catch_test_macros.hpp>

#include "json/json_value.h"

using mqtt::json::JsonValue;

// Pins the JSON wire format MCP clients rely on: no decimal point on
// integral doubles, and the standard escapes for quote, backslash,
// newline, and tab. A regression here breaks every MCP consumer parsing
// engine-tester output, not just this repo's own tests.
TEST_CASE("wire_format_integral_doubles_stringify_without_decimal_point", "[json][wire-format]") {
    CHECK(JsonValue{42.0}.stringify() == "42");
    CHECK(JsonValue{0.0}.stringify() == "0");
    CHECK(JsonValue{-7.0}.stringify() == "-7");
    CHECK(JsonValue{2.5}.stringify() == "2.5");
}

TEST_CASE("wire_format_string_escaping_matches_wire_expectations", "[json][wire-format]") {
    CHECK(JsonValue{std::string{"\""}}.stringify() == "\"\\\"\"");
    CHECK(JsonValue{std::string{"\\"}}.stringify() == "\"\\\\\"");
    CHECK(JsonValue{std::string{"\n"}}.stringify() == "\"\\n\"");
    CHECK(JsonValue{std::string{"\t"}}.stringify() == "\"\\t\"");
}
