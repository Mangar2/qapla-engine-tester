#include <catch2/catch_test_macros.hpp>

#include "../../base-elements/table-format.h"

using QaplaTester::TableCell;
using QaplaTester::TableData;
using QaplaTester::TableFormat;

TEST_CASE("table_format_json_builds_column_and_row_shape", "[json-migration][table-format]") {
    TableData table;
    table.headers = {"Name", "Score"};
    table.columnWidths = {10, 5};
    table.body.push_back({TableCell("engineA"), TableCell(1.5)});
    table.body.push_back({TableCell("engineB"), TableCell(2)});

    const auto json = TableFormat::toJsonValue("ratingTable", table);

    REQUIRE(json.is_object());
    CHECK(json.at("type").as_string() == "table");
    CHECK(json.at("name").as_string() == "ratingTable");

    const auto& columns = json.at("columns");
    REQUIRE(columns.is_array());
    REQUIRE(columns.size() == 2U);
    CHECK(columns.at(0U).at("header").as_string() == "Name");
    CHECK(columns.at(0U).at("width").as_number() == 10.0);

    const auto& rows = json.at("rows");
    REQUIRE(rows.is_array());
    REQUIRE(rows.size() == 2U);
    CHECK(rows.at(0U).at(0U).as_string() == "engineA");
    CHECK(rows.at(0U).at(1U).as_number() == 1.5);
    CHECK(rows.at(1U).at(1U).as_number() == 2.0);
}

TEST_CASE("table_format_json_rounds_cp_columns_to_one_decimal", "[json-migration][table-format]") {
    TableData table;
    table.headers = {"cp"};
    table.body.push_back({TableCell(12.3456)});

    const auto json = TableFormat::toJsonValue("epdStatus", table);

    CHECK(json.at("rows").at(0U).at(0U).as_number() == 12.3);
}

TEST_CASE("table_format_json_escapes_quotes_and_newlines_in_strings", "[json-migration][table-format]") {
    TableData table;
    table.headers = {"Name"};
    table.body.push_back({TableCell(std::string("engine \"X\"\nnext"))});

    const auto json = TableFormat::toJsonValue("outcome", table);
    const auto serialized = json.stringify();

    CHECK(serialized.find("\\\"") != std::string::npos);
    CHECK(serialized.find("\\n") != std::string::npos);

    const auto reparsed = QaplaTester::Json::JsonValue::parse(serialized);
    CHECK(reparsed.at("rows").at(0U).at(0U).as_string() == "engine \"X\"\nnext");
}

TEST_CASE("table_format_json_handles_empty_table", "[json-migration][table-format]") {
    TableData table;
    const auto json = TableFormat::toJsonValue("empty", table);

    REQUIRE(json.at("columns").is_array());
    REQUIRE(json.at("rows").is_array());
    CHECK(json.at("columns").size() == 0U);
    CHECK(json.at("rows").size() == 0U);
}
