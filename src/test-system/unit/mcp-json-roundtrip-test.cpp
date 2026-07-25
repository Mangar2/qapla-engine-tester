#include <catch2/catch_test_macros.hpp>

#include "../../base-elements/qapla-json.h"

using QaplaTester::Json::JsonValue;

// McpServer's request/response handling is exercised end-to-end by the
// Python integration suite (test/integration/mcp), which drives the real
// process over stdio. McpServer's own methods are private statics wired to
// process-wide singletons (Settings::Manager, EngineWorkerFactory, ...), so
// they aren't a good unit-test target. This test instead pins the JSON-RPC
// shape/self-consistency at the library level: a canonical request is
// parsed, its dispatched fields are read out exactly as mcp-server.cpp
// reads them, a response is built the same way mcp-server.cpp builds one,
// and the response is serialized and re-parsed with no string comparisons.
TEST_CASE("mcp_json_rpc_tool_call_request_round_trips_through_the_wire", "[json-migration][mcp-json-roundtrip]") {
    const std::string requestText = R"({
        "jsonrpc": "2.0",
        "id": 7,
        "method": "tools/call",
        "params": {
            "name": "manage_engines",
            "arguments": {
                "command": "list",
                "engine_tc": "40/60"
            }
        }
    })";

    const auto request = JsonValue::parse(requestText);
    REQUIRE(request.is_object());
    CHECK(request.at("method").as_string() == "tools/call");

    const auto& params = request.at("params");
    REQUIRE(params.is_object());
    const auto& arguments = params.at("arguments");
    REQUIRE(arguments.contains("command"));
    CHECK(arguments.at("command").as_string() == "list");

    // Build a response the same way mcp-server.cpp does: jsonrpc/id echoed,
    // result.content as an array of {type, text} objects, result.isError bool.
    auto response = JsonValue::object();
    response["jsonrpc"] = "2.0";
    response["id"] = request.at("id");

    auto& result = response["result"] = JsonValue::object();
    auto& content = result["content"] = JsonValue::array();
    auto& textContent = content[0U];
    textContent["type"] = "text";
    textContent["text"] = "Registered Engines:\n- EngineA\n";
    result["isError"] = false;

    const auto wireText = response.stringify();
    const auto reparsed = JsonValue::parse(wireText);

    CHECK(reparsed.at("id").as_number() == request.at("id").as_number());
    CHECK_FALSE(reparsed.at("result").at("isError").as_boolean());
    CHECK(reparsed.at("result").at("content").at(0U).at("type").as_string() == "text");
    CHECK(reparsed.at("result").at("content").at(0U).at("text").as_string() ==
        textContent.at("text").as_string());
}

TEST_CASE("mcp_json_rpc_malformed_request_is_rejected_by_the_parser", "[json-migration][mcp-json-roundtrip]") {
    // mcp-message-channel.cpp uses try_parse at the stdin boundary; a
    // malformed frame must fail to parse rather than silently succeed, so
    // the server can respond with a JSON-RPC parse error (-32700).
    const auto parsed = JsonValue::try_parse(R"({"jsonrpc": "2.0", "method": )");
    CHECK_FALSE(parsed.has_value());
}
