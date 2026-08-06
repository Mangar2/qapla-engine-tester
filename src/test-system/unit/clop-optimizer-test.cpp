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
#include "unit-test-helpers.h"
#include "../../engine-handling/engine-config.h"
#include "../../clop/clop-engine-selection.h"
#include "../../clop/clop-optimizer.h"
#include "../../game-manager/game-manager-pool.h"
#include "../../base-elements/app-error.h"

using namespace QaplaTester;
using namespace QaplaTester::Test;

TEST_CASE("CLOPOptimizer initialization edge cases", "[clop][optimizer]") {
    EngineConfig engine;
    engine.setName("TestEngine");
    const std::vector<EngineConfig> engines = { engine };
    
    CLOPConfig config;
    config.openingsFile = testOpeningsFilePath();
    config.samples = 10;
    config.gamesPerSample = 8;
    config.parameters.push_back({"Param1", 0.0, 20.0});

    CLOPOptimizer optimizer;

    SECTION("Missing parameters throws") {
        auto badConfig = config;
        badConfig.parameters.clear();
        REQUIRE_THROWS_AS(optimizer.createCLOP(engines, badConfig), AppError);
    }
    
    SECTION("Zero samples throws") {
        auto badConfig = config;
        badConfig.samples = 0;
        REQUIRE_THROWS_AS(optimizer.createCLOP(engines, badConfig), AppError);
    }

    SECTION("Zero games per sample throws") {
        auto badConfig = config;
        badConfig.gamesPerSample = 0;
        REQUIRE_THROWS_AS(optimizer.createCLOP(engines, badConfig), AppError);
    }

    SECTION("Missing openings file throws") {
        auto badConfig = config;
        badConfig.openingsFile = "";
        REQUIRE_THROWS_AS(optimizer.createCLOP(engines, badConfig), AppError);
    }

    SECTION("Invalid openings file throws") {
        auto badConfig = config;
        badConfig.openingsFile = "nonexistent_file.pgn";
        REQUIRE_THROWS_AS(optimizer.createCLOP(engines, badConfig), AppError);
    }

    SECTION("No engines throws") {
        const std::vector<EngineConfig> noEngines;
        REQUIRE_THROWS_AS(optimizer.createCLOP(noEngines, config), AppError);
    }

    SECTION("Multiple gauntlet engines throw") {
        auto gauntletA = engine;
        gauntletA.setName("GauntletA");
        gauntletA.setGauntlet(true);
        auto gauntletB = engine;
        gauntletB.setName("GauntletB");
        gauntletB.setGauntlet(true);
        const std::vector<EngineConfig> badEngines = { gauntletA, gauntletB };
        REQUIRE_THROWS_AS(optimizer.createCLOP(badEngines, config), AppError);
    }

    SECTION("Uninitialized schedule throws") {
        GameManagerPool pool;
        REQUIRE_THROWS_AS(optimizer.scheduleCLOP(1, pool), AppError);
    }
}

TEST_CASE("CLOPOptimizer valid initialization", "[clop][optimizer]") {
    EngineConfig engine;
    engine.setName("TestEngine");
    const std::vector<EngineConfig> engines = { engine };
    
    CLOPConfig config;
    config.openingsFile = testOpeningsFilePath();
    config.samples = 10;
    config.gamesPerSample = 8;
    
    SECTION("Single parameter initialization uses midpoint") {
        config.parameters.push_back({"TestParam", 0.0, 100.0});
        
        CLOPOptimizer optimizer;
        REQUIRE_NOTHROW(optimizer.createCLOP(engines, config));
        
        REQUIRE(optimizer.getCompletedSamples() == 0);
        
        auto params = optimizer.getEstimatedParameters();
        REQUIRE(params.size() == 1);
        REQUIRE(params[0] == 50.0);
    }
    
    SECTION("Multiple parameter initialization") {
        config.parameters.push_back({"Param1", 0.0, 20.0});
        config.parameters.push_back({"Param2", 25.0, 75.0});
        config.parameters.push_back({"Param3", 50.0, 150.0});
        
        CLOPOptimizer optimizer;
        REQUIRE_NOTHROW(optimizer.createCLOP(engines, config));
        
        auto params = optimizer.getEstimatedParameters();
        REQUIRE(params.size() == 3);
        REQUIRE(params[0] == 10.0);
        REQUIRE(params[1] == 50.0);
        REQUIRE(params[2] == 100.0);
    }
}

TEST_CASE("CLOP engine role selection", "[clop][optimizer][roles]") {
    using QaplaTester::Clop::resolveOptimizingEngineIndex;

    SECTION("First engine is optimizer when no gauntlet is set") {
        const auto engines = createEngines({
            { .name = "A" },
            { .name = "B" },
            { .name = "C" },
        });

        REQUIRE(resolveOptimizingEngineIndex(engines) == 0U);
    }

    SECTION("Gauntlet engine is optimizer") {
        const auto engines = createEngines({
            { .name = "A" },
            { .name = "B", .isGauntlet = true },
            { .name = "C" },
        });

        REQUIRE(resolveOptimizingEngineIndex(engines) == 1U);
    }

    SECTION("No engines throws") {
        const std::vector<EngineConfig> engines;
        REQUIRE_THROWS_AS(resolveOptimizingEngineIndex(engines), AppError);
    }

    SECTION("Multiple gauntlets throw") {
        const auto engines = createEngines({
            { .name = "A", .isGauntlet = true },
            { .name = "B", .isGauntlet = true },
        });

        REQUIRE_THROWS_AS(resolveOptimizingEngineIndex(engines), AppError);
    }
}

TEST_CASE("CLOP opponent round robin", "[clop][optimizer][rotation]") {
    using QaplaTester::Clop::createOpponentEngines;
    using QaplaTester::Clop::nextOpponentIndex;

    SECTION("Single engine keeps self as opponent") {
        const auto engines = createEngines({
            { .name = "G" },
        });
        const auto opponents = createOpponentEngines(engines, 0U);

        REQUIRE(opponents.size() == 1U);
        REQUIRE(opponents[0].getName() == "G");
    }

    SECTION("Optimizer is excluded from opponents") {
        const auto engines = createEngines({
            { .name = "G", .isGauntlet = true },
            { .name = "A" },
            { .name = "B" },
            { .name = "C" },
        });
        const auto opponents = createOpponentEngines(engines, 0U);

        REQUIRE(opponents.size() == 3U);
        REQUIRE(opponents[0].getName() == "A");
        REQUIRE(opponents[1].getName() == "B");
        REQUIRE(opponents[2].getName() == "C");
    }

    SECTION("Round robin index advances and wraps") {
        size_t index = 0U;
        index = nextOpponentIndex(index, 3U);
        REQUIRE(index == 1U);
        index = nextOpponentIndex(index, 3U);
        REQUIRE(index == 2U);
        index = nextOpponentIndex(index, 3U);
        REQUIRE(index == 0U);
    }

    SECTION("Round robin fails on zero opponents") {
        REQUIRE_THROWS_AS(nextOpponentIndex(0U, 0U), AppError);
    }
}
