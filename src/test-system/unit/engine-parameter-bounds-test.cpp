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

#include "../../engine-handling/engine-parameter-bounds.h"
#include "../../base-elements/app-error.h"

using namespace QaplaTester;

namespace {

[[nodiscard]] EngineOption makeSpinOption(const std::string& name, int min, int max) {
    EngineOption option;
    option.name = name;
    option.type = EngineOption::Type::Spin;
    option.min = min;
    option.max = max;
    return option;
}

} // namespace

TEST_CASE("Optimizer parameter ranges are checked against engine option bounds", "[engine][options][bounds]") {
    const EngineOptions supportedOptions = { makeSpinOption("PawnValue", 50, 150) };

    SECTION("Range inside the engine bounds is accepted") {
        const std::vector<OptimizerParameterRange> ranges = { { "PawnValue", 60.0, 140.0 } };
        REQUIRE_NOTHROW(validateParameterRanges(supportedOptions, ranges, "TestEngine", "SPSA"));
    }

    SECTION("Range equal to the engine bounds is accepted") {
        const std::vector<OptimizerParameterRange> ranges = { { "PawnValue", 50.0, 150.0 } };
        REQUIRE_NOTHROW(validateParameterRanges(supportedOptions, ranges, "TestEngine", "SPSA"));
    }

    SECTION("Option name is matched case insensitively") {
        const std::vector<OptimizerParameterRange> ranges = { { "pawnvalue", 60.0, 140.0 } };
        REQUIRE_NOTHROW(validateParameterRanges(supportedOptions, ranges, "TestEngine", "SPSA"));
    }

    SECTION("Minimum below the engine bound throws") {
        const std::vector<OptimizerParameterRange> ranges = { { "PawnValue", 20.0, 140.0 } };
        REQUIRE_THROWS_AS(validateParameterRanges(supportedOptions, ranges, "TestEngine", "SPSA"), AppError);
    }

    SECTION("Maximum above the engine bound throws") {
        const std::vector<OptimizerParameterRange> ranges = { { "PawnValue", 60.0, 200.0 } };
        REQUIRE_THROWS_AS(validateParameterRanges(supportedOptions, ranges, "TestEngine", "CLOP"), AppError);
    }

    SECTION("Violation is reported as invalid parameters") {
        const std::vector<OptimizerParameterRange> ranges = { { "PawnValue", 60.0, 200.0 } };
        try {
            validateParameterRanges(supportedOptions, ranges, "TestEngine", "CLOP");
            FAIL("Expected an AppError");
        }
        catch (const AppError& error) {
            CHECK(error.getReturnCode() == AppReturnCode::InvalidParameters);
        }
    }

    SECTION("Unknown option does not throw") {
        const std::vector<OptimizerParameterRange> ranges = { { "UnknownParam", 0.0, 1000.0 } };
        REQUIRE_NOTHROW(validateParameterRanges(supportedOptions, ranges, "TestEngine", "SPSA"));
    }

    SECTION("Option without bounds does not throw") {
        EngineOption stringOption;
        stringOption.name = "PawnValue";
        stringOption.type = EngineOption::Type::String;
        const EngineOptions optionsWithoutBounds = { stringOption };
        const std::vector<OptimizerParameterRange> ranges = { { "PawnValue", 0.0, 1000.0 } };
        REQUIRE_NOTHROW(validateParameterRanges(optionsWithoutBounds, ranges, "TestEngine", "SPSA"));
    }

    SECTION("Unknown engine options skip the check") {
        const std::vector<OptimizerParameterRange> ranges = { { "PawnValue", 0.0, 1000.0 } };
        REQUIRE_NOTHROW(validateParameterRanges({}, ranges, "TestEngine", "SPSA"));
    }

    SECTION("Only the upper bound defined by the engine is checked") {
        EngineOption option;
        option.name = "PawnValue";
        option.type = EngineOption::Type::Spin;
        option.max = 150;
        const EngineOptions upperBoundOnly = { option };

        REQUIRE_NOTHROW(validateParameterRanges(upperBoundOnly, { { "PawnValue", -100.0, 150.0 } }, "TestEngine", "SPSA"));
        REQUIRE_THROWS_AS(validateParameterRanges(upperBoundOnly, { { "PawnValue", 0.0, 151.0 } }, "TestEngine", "SPSA"), AppError);
    }
}
