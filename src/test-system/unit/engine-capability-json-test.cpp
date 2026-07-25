#include <catch2/catch_test_macros.hpp>

#include "../../engine-handling/engine-capability.h"

#include <sstream>

using QaplaConfiguration::EngineCapability;
using QaplaHelpers::IniFile;

namespace {

[[nodiscard]] IniFile::Section makeBaseSection() {
    IniFile::Section section;
    section.addEntry("path", "/usr/bin/engine");
    section.addEntry("protocol", "uci");
    section.addEntry("name", "TestEngine");
    section.addEntry("author", "Test Author");
    return section;
}

} // namespace

TEST_CASE("engine_capability_reads_old_spaced_json_format_option", "[json-migration][engine-capability]") {
    // Verbatim sample of the format written by the pre-migration hand-rolled
    // serializer (spaces after ':' and ','): persisted capability caches
    // written by older builds must keep parsing correctly.
    auto section = makeBaseSection();
    section.addEntry("option.Hash",
        R"({"name": "Hash", "type": "spin", "defaultValue": "16", "min": 1, "max": 4096})");

    const auto capability = EngineCapability::createFromSection(section);

    REQUIRE(capability.getSupportedOptions().size() == 1U);
    const auto& option = capability.getSupportedOptions().front();
    CHECK(option.name == "Hash");
    CHECK(option.type == QaplaTester::EngineOption::Type::Spin);
    CHECK(option.defaultValue == "16");
    REQUIRE(option.min.has_value());
    CHECK(*option.min == 1);
    REQUIRE(option.max.has_value());
    CHECK(*option.max == 4096);
}

TEST_CASE("engine_capability_reads_old_spaced_json_format_vars_array", "[json-migration][engine-capability]") {
    auto section = makeBaseSection();
    section.addEntry("option.Style",
        R"({"name": "Style", "type": "combo", "defaultValue": "Normal", "vars": ["Solid", "Normal", "Risky"]})");

    const auto capability = EngineCapability::createFromSection(section);

    REQUIRE(capability.getSupportedOptions().size() == 1U);
    const auto& option = capability.getSupportedOptions().front();
    REQUIRE(option.vars.size() == 3U);
    CHECK(option.vars[0] == "Solid");
    CHECK(option.vars[1] == "Normal");
    CHECK(option.vars[2] == "Risky");
}

TEST_CASE("engine_capability_reads_option_with_optional_fields_absent", "[json-migration][engine-capability]") {
    auto section = makeBaseSection();
    section.addEntry("option.Ponder", R"({"name":"Ponder","type":"check"})");

    const auto capability = EngineCapability::createFromSection(section);

    REQUIRE(capability.getSupportedOptions().size() == 1U);
    const auto& option = capability.getSupportedOptions().front();
    CHECK(option.name == "Ponder");
    CHECK(option.type == QaplaTester::EngineOption::Type::Check);
    CHECK(option.defaultValue.empty());
    CHECK_FALSE(option.min.has_value());
    CHECK_FALSE(option.max.has_value());
    CHECK(option.vars.empty());
}

TEST_CASE("engine_capability_ignores_malformed_option_entry", "[json-migration][engine-capability]") {
    auto section = makeBaseSection();
    section.addEntry("option.Broken", "not json at all");

    // Malformed individual option lines are skipped, not fatal for the whole capability.
    const auto capability = EngineCapability::createFromSection(section);
    CHECK(capability.getSupportedOptions().empty());
    CHECK(capability.getPath() == "/usr/bin/engine");
}

TEST_CASE("engine_capability_save_round_trips_through_the_new_compact_format", "[json-migration][engine-capability]") {
    auto section = makeBaseSection();
    section.addEntry("option.Hash",
        R"({"name": "Hash", "type": "spin", "defaultValue": "16", "min": 1, "max": 4096})");
    const auto original = EngineCapability::createFromSection(section);

    std::ostringstream out;
    original.save(out);
    const auto saved = out.str();

    // New serializer output is compact (no spaces after ':'/','). Object
    // keys serialize in sorted order (std::map-backed), not insertion order.
    CHECK(saved.find(R"("name":"Hash")") != std::string::npos);
    CHECK(saved.find(R"(": )") == std::string::npos);
    CHECK(saved.find(R"(, ")") == std::string::npos);

    const auto optionLinePos = saved.find("option.Hash=");
    REQUIRE(optionLinePos != std::string::npos);
    const auto lineEnd = saved.find('\n', optionLinePos);
    const auto optionLine = saved.substr(optionLinePos + std::string("option.Hash=").size(),
        lineEnd - optionLinePos - std::string("option.Hash=").size());

    IniFile::Section reparsedSection = makeBaseSection();
    reparsedSection.addEntry("option.Hash", optionLine);
    const auto roundTripped = EngineCapability::createFromSection(reparsedSection);

    REQUIRE(roundTripped.getSupportedOptions().size() == 1U);
    const auto& option = roundTripped.getSupportedOptions().front();
    CHECK(option.name == "Hash");
    CHECK(option.defaultValue == "16");
    REQUIRE(option.min.has_value());
    CHECK(*option.min == 1);
    REQUIRE(option.max.has_value());
    CHECK(*option.max == 4096);
}
