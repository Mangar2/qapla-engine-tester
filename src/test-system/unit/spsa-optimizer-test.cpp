/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include <catch2/catch_test_macros.hpp>
#include "spsa-test-helpers.h"
#include "../../engine-config.h"
#include "../../spsa-optimizer.h"
#include "../../game-manager-pool.h"

using namespace QaplaTester;
using namespace QaplaTester::Test;

TEST_CASE("SPSA Optimizer initialization and configuration", "[spsa][optimizer]") {
    EngineConfig engine;
    engine.setName("TestEngine");
    
    SPSAConfig config;
    config.openingsFile = "src/test-system/unit/test-openings.pgn";
    config.maxActivePairs = 2;
    config.gamesPerPair = 8;
    config.iterations = 5;
    config.learningRate = 0.01;
    config.swapColors = true;
    
    SECTION("Single parameter initialization") {
        SPSAParameterConfig param;
        param.name = "TestParam";
        param.defaultValue = 50.0;
        param.minValue = 0.0;
        param.maxValue = 100.0;
        param.c = 5.0;
        config.parameters.push_back(param);
        
        SPSABuilder builder(engine, config);
        
        REQUIRE(builder.perturbationCount() == 2);
        REQUIRE(builder.getCompletedIterations() == 0);
        
        auto params = builder.getCurrentParameters();
        REQUIRE(params.size() == 1);
        REQUIRE(params[0] == 50.0);
    }
    
    SECTION("Multiple parameters") {
        config.parameters.push_back({"Param1", 10.0, 0.0, 20.0, 1.0});
        config.parameters.push_back({"Param2", 50.0, 25.0, 75.0, 2.5});
        config.parameters.push_back({"Param3", 100.0, 50.0, 150.0, 5.0});
        
        SPSABuilder builder(engine, config);
        
        auto params = builder.getCurrentParameters();
        REQUIRE(params.size() == 3);
        REQUIRE(params[0] == 10.0);
        REQUIRE(params[1] == 50.0);
        REQUIRE(params[2] == 100.0);
    }
    
    SECTION("Parameter bounds") {
        SPSAParameterConfig param;
        param.name = "BoundedParam";
        param.defaultValue = 75.0;
        param.minValue = 50.0;
        param.maxValue = 100.0;
        param.c = 10.0;
        config.parameters.push_back(param);
        
        SPSABuilder builder(engine, config);
        
        auto params = builder.getCurrentParameters();
        REQUIRE(params[0] == 75.0);
        CHECK(param.minValue <= params[0]);
        CHECK(params[0] <= param.maxValue);
    }
    
    SECTION("Different learning rates") {
        config.parameters.push_back({"TestParam", 50.0, 0.0, 100.0, 5.0});
        
        std::vector<double> learningRates = {0.001, 0.01, 0.1, 1.0};
        
        for (double lr : learningRates) {
            config.learningRate = lr;
            SPSABuilder builder(engine, config);
            CHECK(config.learningRate == lr);
        }
    }
}

TEST_CASE("SPSA PairTournament configuration is correct", "[spsa][config]") {
    EngineConfig engine;
    engine.setName("TestEngine");
    
    SPSAConfig config;
    config.openingsFile = "src/test-system/unit/test-openings.pgn";
    config.maxActivePairs = 2;
    config.gamesPerPair = 8;
    config.iterations = 3;
    config.learningRate = 0.1;
    config.swapColors = true;
    config.openingsSeed = 42;
    
    SPSAParameterConfig param;
    param.name = "Mobility";
    param.defaultValue = 100.0;
    param.minValue = 50.0;
    param.maxValue = 150.0;
    param.c = 10.0;
    config.parameters.push_back(param);
    
    SPSABuilder builder(engine, config);
    
    REQUIRE(builder.perturbationCount() == 2);
    
    // Get the first pair tournament
    auto pairOpt = builder.optimizer.getPairTournament(0);
    REQUIRE(pairOpt.has_value());
    
    auto* pair = *pairOpt;
    REQUIRE(pair != nullptr);
    
    // Check that swapColors is correctly set in the pair tournament config
    auto pairConfig = pair->getConfig();
    REQUIRE(pairConfig.swapColors == true);
    REQUIRE(pairConfig.games == 8);
}

TEST_CASE("SPSA with balanced game results keeps parameters unchanged", "[spsa][games]") {
    EngineConfig engine;
    engine.setName("OptimizingEngine");
    
    SPSAConfig config;
    config.openingsFile = "src/test-system/unit/test-openings.pgn";
    config.maxActivePairs = 2;
    config.gamesPerPair = 8;
    config.iterations = 3;
    config.learningRate = 0.1;
    config.swapColors = true;  // With swapColors, WhiteWins leads to 4:4 split
    config.openingsSeed = 42;
    
    SPSAParameterConfig param;
    param.name = "Mobility";
    param.defaultValue = 100.0;
    param.minValue = 50.0;
    param.maxValue = 150.0;
    param.c = 10.0;
    config.parameters.push_back(param);
    
    SPSABuilder builder(engine, config);
    
    // Get initial parameter values
    auto initialParams = builder.getCurrentParameters();
    REQUIRE(initialParams.size() == 1);
    REQUIRE(initialParams[0] == 100.0);
    
    // createSPSA should have created perturbations
    REQUIRE(builder.perturbationCount() == 2);
    
    // Play all games with WhiteWins result
    // With swapColors=true: Engine A wins games 0,2,4,6 and Engine B wins games 1,3,5,7
    // This leads to winsEngineA=4, winsEngineB=4, gradient=0
    size_t gamesPlayed = builder.completePerturbationWithWins(0);
    REQUIRE(gamesPlayed == config.gamesPerPair);
    
    // After completing games, check that one iteration is complete
    REQUIRE(builder.getCompletedIterations() == 1);
    
    // Parameters should remain unchanged because gradient is 0 (balanced 4:4 result)
    auto finalParams = builder.getCurrentParameters();
    REQUIRE(finalParams.size() == 1);
    REQUIRE(finalParams[0] == 100.0);
}
