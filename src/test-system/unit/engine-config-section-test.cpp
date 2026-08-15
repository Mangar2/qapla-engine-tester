/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include <catch2/catch_test_macros.hpp>

#include "../../cli/settings-definitions.h"
#include "../../cli/settings-manager.h"
#include "../../engine-handling/engine-config.h"

using namespace QaplaTester;

namespace {

void registerSettingsOnce() {
    static bool registered = []() {
        Settings::initSettings();
        return true;
    }();
    (void)registered;
}

[[nodiscard]] QaplaHelpers::IniFile::Section engineSection(
    const std::string& optionKey, const std::string& optionValue) {
    QaplaHelpers::IniFile::Section section;
    section.name = "engine";
    section.addEntry("id", "tournament");
    section.addEntry("name", "TestEngine");
    section.addEntry("cmd", "/bin/sh");
    section.addEntry(optionKey, optionValue);
    return section;
}

} // namespace

TEST_CASE("Engine sections round-trip their UCI options", "[engine][config][section]") {
    registerSettingsOnce();

    SECTION("An option written by toSection() is read back as that option") {
        // toSection() spells options with the "option." prefix the parameter definition requires.
        // Reading the prefix as part of the name produced an option called "option.Hash": the
        // engine never got the setting, and saving again wrote "option.option.Hash".
        const auto config = EngineConfig::createFromSection(engineSection("option.Hash", "64"));
        const auto options = config.getOptionValues();

        REQUIRE(options.contains("Hash"));
        CHECK(options.at("Hash") == "64");
        CHECK(options.size() == 1);

        const auto section = config.toSection("engine");
        CHECK(section.getValue("option.Hash").value() == "64");
        CHECK_FALSE(section.getValue("option.option.Hash").has_value());
    }

    SECTION("An option without the prefix is still accepted") {
        // Engine configuration files have always named options directly.
        const auto options = EngineConfig::createFromSection(
            engineSection("Hash", "64")).getOptionValues();

        REQUIRE(options.contains("Hash"));
        CHECK(options.at("Hash") == "64");
    }

    SECTION("The prefix is matched case-insensitively, the option name is kept as written") {
        const auto config = EngineConfig::createFromSection(engineSection("OPTION.MultiPV", "2"));
        const auto options = config.getOptionValues();

        REQUIRE(options.contains("MultiPV"));
        CHECK(options.at("MultiPV") == "2");
        CHECK(config.toSection("engine").getValue("option.MultiPV").value() == "2");
    }
}

TEST_CASE("Engine sections omit the name reported by the engine", "[engine][config][section]") {
    registerSettingsOnce();

    // The reported name is discovered from the running engine and therefore not persisted.
    // toSection() applies that rule while walking the central parameter definition, which
    // registers every key in lower case - so a rule comparing against "originalName" stops
    // matching without anyone noticing, and the key lands in tournament and SPRT files.
    auto config = EngineConfig::createFromSection(engineSection("option.Hash", "64"));
    config.setReportedName("Qapla 0.4.0");
    REQUIRE(config.getReportedName() == "Qapla 0.4.0");

    const auto section = config.toSection("engine");

    CHECK_FALSE(section.getValue("originalname").has_value());
    CHECK_FALSE(section.getValue("originalName").has_value());
    // The name the engine is configured under is still written.
    CHECK(section.getValue("name").value() == "TestEngine");
}
