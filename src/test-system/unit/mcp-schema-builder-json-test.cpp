#include <catch2/catch_test_macros.hpp>

#include "../../mcp/mcp-schema-builder.h"

using QaplaTester::Mcp::McpSchemaBuilder;

// "read_report" is the simplest tool schema: it doesn't consult the
// Settings::Manager group/global definitions, so this test needs no
// application-wide settings initialization.
TEST_CASE("schema_builder_read_report_schema_has_documented_shape", "[json-migration][mcp-schema-builder]") {
    const McpSchemaBuilder::ToolInfo info{
        .name = "read_report",
        .description = "Reads the content of a specific report log or PGN file",
        .groups = {}
    };

    const auto schema = McpSchemaBuilder::createInputSchema(info, "");

    REQUIRE(schema.is_object());
    CHECK(schema.at("type").as_string() == "object");

    const auto& properties = schema.at("properties");
    REQUIRE(properties.is_object());
    REQUIRE(properties.contains("uri"));
    CHECK(properties.at("uri").at("type").as_string() == "string");

    const auto& required = schema.at("required");
    REQUIRE(required.is_array());
    REQUIRE(required.size() == 1U);
    CHECK(required.at(0U).as_string() == "uri");

    // Must be valid, self-consistent JSON.
    const auto reparsed = QaplaTester::Json::JsonValue::parse(schema.stringify());
    CHECK(reparsed.at("properties").at("uri").at("type").as_string() == "string");
}

TEST_CASE("schema_builder_control_schema_lists_command_enum", "[json-migration][mcp-schema-builder]") {
    const McpSchemaBuilder::ToolInfo info{
        .name = "control",
        .description = "Control running tasks and queued jobs",
        .groups = {}
    };

    const auto schema = McpSchemaBuilder::createInputSchema(info, "");

    const auto& command = schema.at("properties").at("command");
    REQUIRE(command.at("enum").is_array());
    CHECK(command.at("enum").size() == 8U);
    CHECK(command.at("enum").at(0U).as_string() == "status");
}
