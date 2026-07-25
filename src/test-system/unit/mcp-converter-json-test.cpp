#include <catch2/catch_test_macros.hpp>

#include "../../mcp/mcp-converter.h"
#include "../../base-elements/app-error.h"

using QaplaTester::AppError;
using QaplaTester::Json::JsonValue;
using QaplaTester::Mcp::convertJsonToEngineSetting;
using QaplaTester::Mcp::validateAndToString;

// convertJsonToEngineSetting resolves keys via Settings::getEngineKeys(), a
// pure function independent of Settings::Manager registration state, so
// these tests don't need the manager singleton initialized.

TEST_CASE("convert_json_to_engine_setting_handles_bool_string_and_path_types", "[json-migration][mcp-converter]") {
    const auto ponder = convertJsonToEngineSetting("ponder", JsonValue{true});
    REQUIRE(std::holds_alternative<bool>(ponder));
    CHECK(std::get<bool>(ponder));

    const auto timeControl = convertJsonToEngineSetting("tc", JsonValue{"40/60"});
    REQUIRE(std::holds_alternative<std::string>(timeControl));
    CHECK(std::get<std::string>(timeControl) == "40/60");

    const auto cmd = convertJsonToEngineSetting("cmd", JsonValue{"/usr/bin/engine"});
    REQUIRE(std::holds_alternative<std::string>(cmd));
    CHECK(std::get<std::string>(cmd) == "/usr/bin/engine");
}

TEST_CASE("convert_json_to_engine_setting_throws_on_type_mismatch", "[json-migration][mcp-converter]") {
    // "ponder" is schema-typed Bool; passing a string must be rejected, not coerced.
    CHECK_THROWS_AS(convertJsonToEngineSetting("ponder", JsonValue{"true"}), AppError);
    // "tc" is schema-typed String; passing a number must be rejected.
    CHECK_THROWS_AS(convertJsonToEngineSetting("tc", JsonValue{5.0}), AppError);
}

TEST_CASE("convert_json_to_engine_setting_throws_on_unknown_key", "[json-migration][mcp-converter]") {
    CHECK_THROWS_AS(convertJsonToEngineSetting("not_a_real_engine_key", JsonValue{"x"}), AppError);
}

TEST_CASE("convert_json_to_engine_setting_handles_dynamic_uci_options", "[json-migration][mcp-converter]") {
    const auto hashOption = convertJsonToEngineSetting("option_Hash", JsonValue{128.0});
    REQUIRE(std::holds_alternative<std::string>(hashOption));
    CHECK(std::get<std::string>(hashOption) == "128");
}

TEST_CASE("validate_and_to_string_converts_scalar_json_types_for_unknown_keys", "[json-migration][mcp-converter]") {
    // Fallback path (key unknown to Settings::Manager) still coerces scalar
    // JSON values to strings without touching the manager singleton.
    CHECK(validateAndToString("custom_flag", JsonValue{true}) == "true");
    CHECK(validateAndToString("custom_flag", JsonValue{false}) == "false");
    CHECK(validateAndToString("custom_text", JsonValue{"hello"}) == "hello");
    CHECK(validateAndToString("custom_int", JsonValue{42.0}) == "42");
    CHECK(validateAndToString("custom_float", JsonValue{2.5}) == std::to_string(2.5));
}

TEST_CASE("validate_and_to_string_throws_for_unsupported_value_type", "[json-migration][mcp-converter]") {
    CHECK_THROWS_AS(validateAndToString("custom_object", JsonValue::object()), AppError);
}
