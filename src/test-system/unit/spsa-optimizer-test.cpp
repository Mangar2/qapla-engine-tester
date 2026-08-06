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
#include <catch2/catch_approx.hpp>
#include "spsa-test-helpers.h"
#include "unit-test-helpers.h"
#include "../../engine-handling/engine-config.h"
#include "../../spsa/spsa-optimizer.h"
#include "../../game-manager/game-manager-pool.h"

using namespace QaplaTester;
using namespace QaplaTester::Test;
using Catch::Approx;

TEST_CASE("SPSA Optimizer initialization and configuration", "[spsa][optimizer]") {
    EngineConfig engine;
    engine.setName("TestEngine");
    
    SPSAConfig config;
    config.openingsFile = testOpeningsFilePath();
    config.maxActivePairs = 2;
    config.gamesPerPair = 8;
    config.iterations = 5;
    config.learningRate = 0.01;

    
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
    config.openingsFile = testOpeningsFilePath();
    config.maxActivePairs = 2;
    config.gamesPerPair = 8;
    config.iterations = 3;
    config.learningRate = 0.1;

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
    

    auto pairConfig = pair->getConfig();

    REQUIRE(pairConfig.games == 8);
}

TEST_CASE("SPSA with balanced game results keeps parameters unchanged", "[spsa][games]") {
    EngineConfig engine;
    engine.setName("OptimizingEngine");
    
    SPSAConfig config;
    config.openingsFile = testOpeningsFilePath();
    config.maxActivePairs = 2;
    config.gamesPerPair = 8;
    config.iterations = 3;
    config.learningRate = 0.1;

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

    // This leads to winsEngineA=4, winsEngineB=4, gradient=0
    size_t gamesPlayed = builder.completePerturbationBalanced(0);
    REQUIRE(gamesPlayed == config.gamesPerPair);
    
    // After completing games, check that one iteration is complete
    REQUIRE(builder.getCompletedIterations() == 1);
    
    // Parameters should remain unchanged because gradient is 0 (balanced 4:4 result)
    auto finalParams = builder.getCurrentParameters();
    REQUIRE(finalParams.size() == 1);
    REQUIRE(finalParams[0] == 100.0);
}

TEST_CASE("SPSA updates all parameters when one engine dominates", "[spsa][games][multiparameter]") {
    EngineConfig engine;
    engine.setName("MultiParamEngine");
    
    SPSAConfig config;
    config.openingsFile = testOpeningsFilePath();
    config.maxActivePairs = 2;
    config.gamesPerPair = 8;
    config.iterations = 3;
    config.learningRate = 0.1;

    config.openingsSeed = 42;
    
    // Create 10 parameters with different c values
    std::vector<std::string> paramNames = {
        "Mobility", "KingSafety", "PawnStructure", "Material", "Development",
        "CenterControl", "PieceActivity", "Threats", "PassedPawns", "Tempo"
    };
    
    std::vector<double> defaultValues = {100.0, 80.0, 90.0, 110.0, 70.0, 85.0, 95.0, 75.0, 105.0, 65.0};
    std::vector<double> cValues = {10.0, 8.0, 9.0, 11.0, 7.0, 8.5, 9.5, 7.5, 10.5, 6.5};
    
    for (size_t i = 0; i < 10; ++i) {
        SPSAParameterConfig param;
        param.name = paramNames[i];
        param.defaultValue = defaultValues[i];
        param.minValue = defaultValues[i] - 50.0;
        param.maxValue = defaultValues[i] + 50.0;
        param.c = cValues[i];
        config.parameters.push_back(param);
    }
    
    SPSABuilder builder(engine, config);
    
    // Get initial parameter values
    auto initialParams = builder.getCurrentParameters();
    REQUIRE(initialParams.size() == 10);
    
    // Verify initial values
    for (size_t i = 0; i < 10; ++i) {
        REQUIRE(initialParams[i] == defaultValues[i]);
    }
    
    // Get the perturbation to access deltas
    auto pairOpt = builder.optimizer.getPairTournament(0);
    REQUIRE(pairOpt.has_value());
    
    // Play all games with WhiteWins result

    // gradient_signal = 8 - 0 = 8
    size_t gamesPlayed = builder.completePerturbationWithWins(0);
    REQUIRE(gamesPlayed == config.gamesPerPair);
    
    // After completing games, check that one iteration is complete
    REQUIRE(builder.getCompletedIterations() == 1);
    
    // Get updated parameters
    auto finalParams = builder.getCurrentParameters();
    REQUIRE(finalParams.size() == 10);
    
    // Verify that ALL parameters have changed
    // Update formula: Δθ_i = r * c_i * gradient_signal * delta_i
    // With r=0.1, gradient_signal=8, delta_i=±1:
    // Δθ_i = 0.1 * c_i * 8 * (±1) = ±0.8 * c_i
    
    for (size_t i = 0; i < 10; ++i) {
        // Parameters must have changed
        REQUIRE(finalParams[i] != initialParams[i]);
        
        // Calculate expected update magnitude: |Δθ_i| = 0.1 * 8 * c_i = 0.8 * c_i
        double expectedUpdateMagnitude = config.learningRate * config.gamesPerPair * cValues[i];
        double actualChange = std::abs(finalParams[i] - initialParams[i]);
        
        // Verify the update magnitude is correct
        REQUIRE(actualChange == Approx(expectedUpdateMagnitude).epsilon(0.001));
        
        // Verify parameters are within bounds
        REQUIRE(finalParams[i] >= config.parameters[i].minValue);
        REQUIRE(finalParams[i] <= config.parameters[i].maxValue);
    }
}

TEST_CASE("SPSA processes multiple active pairs correctly", "[spsa][games][multipair]") {
    EngineConfig engine;
    engine.setName("MultiPairEngine");
    
    SPSAConfig config;
    config.openingsFile = testOpeningsFilePath();
    config.maxActivePairs = 10;
    config.gamesPerPair = 8;
    config.iterations = 10;
    config.learningRate = 0.1;

    config.openingsSeed = 12345;  // Fixed seed for reproducible deltas
    
    // Create 2 parameters, we'll only verify the first one
    SPSAParameterConfig param1;
    param1.name = "FirstParam";
    param1.defaultValue = 100.0;
    param1.minValue = 50.0;
    param1.maxValue = 150.0;
    param1.c = 1.4;
    config.parameters.push_back(param1);
    
    SPSAParameterConfig param2;
    param2.name = "SecondParam";
    param2.defaultValue = 200.0;
    param2.minValue = 150.0;
    param2.maxValue = 250.0;
    param2.c = 2.0;
    config.parameters.push_back(param2);
    
    SPSABuilder builder(engine, config);
    
    // Get initial parameter values
    auto initialParams = builder.getCurrentParameters();
    REQUIRE(initialParams.size() == 2);
    REQUIRE(initialParams[0] == 100.0);
    REQUIRE(initialParams[1] == 200.0);
    
    // Should have created 10 initial perturbations
    REQUIRE(builder.perturbationCount() == 10);
    
    // Extract deltas from all perturbations by reading engine A's option values
    std::vector<int> allDeltas;
    for (size_t i = 0; i < 10; ++i) {
        auto pairOpt = builder.optimizer.getPairTournament(i);
        REQUIRE(pairOpt.has_value());
        
        auto& engineA = (*pairOpt)->getEngineA();
        auto optionValues = engineA.getOptionValues();
        
        // Engine A has perturbed value: default + c * delta
        // FirstParam: default=100, c=1.4
        // So: perturbedValue = 100 + 1.4 * delta
        // Therefore: delta = (perturbedValue - 100) / 1.4
        auto it = optionValues.find("FirstParam");
        REQUIRE(it != optionValues.end());
        
        double perturbedValue = std::stod(it->second);
        int delta = static_cast<int>(std::round((perturbedValue - 100.0) / 1.4));
        allDeltas.push_back(delta);
    }
    
    // Play all games for all 10 perturbations
    // Each perturbation: Engine A wins all 8 games -> gradient_signal = 8
    // Update per iteration: Δθ = 0.1 * 1.4 * 8 * delta_i = 1.12 * delta_i
    for (size_t i = 0; i < 10; ++i) {
        size_t gamesPlayed = builder.completePerturbationWithWins(i);
        REQUIRE(gamesPlayed == config.gamesPerPair);
    }
    
    // After completing all 10 perturbations, check iterations
    REQUIRE(builder.getCompletedIterations() == 10);
    
    // Get final parameters
    auto finalParams = builder.getCurrentParameters();
    REQUIRE(finalParams.size() == 2);
    
    // Calculate expected value for first parameter
    // Starting value: 100.0
    // Each iteration adds: 0.1 * 1.4 * 8 * delta_i = 1.12 * delta_i
    double expectedValue = 100.0;
    for (int delta : allDeltas) {
        expectedValue += config.learningRate * param1.c * config.gamesPerPair * delta;
    }
    
    // Verify the first parameter matches expected value
    REQUIRE(finalParams[0] == Approx(expectedValue).epsilon(0.001));
    
    // Verify parameters stayed within bounds
    REQUIRE(finalParams[0] >= param1.minValue);
    REQUIRE(finalParams[0] <= param1.maxValue);
}
