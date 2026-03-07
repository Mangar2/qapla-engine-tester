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

#include "../../clop/clop-model.h"

#include <algorithm>
#include <cmath>

using namespace QaplaTester;

TEST_CASE("CLOPModel converges to interior optimum on smooth synthetic landscape", "[clop][model]") {
    CLOPConfig config;
    config.parameters.push_back({"Tunable", 50.0, 0.0, 100.0});
    config.maxWeightIterations = 30;
    config.h = 3.0;
    config.priorVariance = 100.0;

    CLOPModel model(config);

    std::vector<CLOPModelSample> samples;
    constexpr double targetValue = 72.0;
    constexpr double sigma = 9.0;
    constexpr double observationWeight = 256.0;

    for (int sampleIndex = 0; sampleIndex <= 80; ++sampleIndex) {
        const double value = static_cast<double>(sampleIndex) * 1.25;
        const double distance = value - targetValue;
        const double outcome = std::exp(-(distance * distance) / (2.0 * sigma * sigma));

        CLOPModelSample sample;
        sample.values = {value};
        sample.normalizedValues = model.normalizeValues(sample.values);
        sample.outcome = std::clamp(outcome, 0.0, 1.0);
        sample.observationWeight = observationWeight;
        samples.push_back(std::move(sample));
    }

    REQUIRE_NOTHROW(model.updateDesignWeights(samples));
    const auto estimated = model.computeEstimatedOptimum(samples);

    REQUIRE(estimated.size() == 1);
    REQUIRE(std::isfinite(estimated[0]));
    REQUIRE(estimated[0] >= 0.0);
    REQUIRE(estimated[0] <= 100.0);
    REQUIRE(estimated[0] > config.parameters[0].defaultValue + 5.0);
    REQUIRE(estimated[0] < 90.0);
}

TEST_CASE("CLOPModel remains finite with high observation weights", "[clop][model][stability]") {
    CLOPConfig config;
    config.parameters.push_back({"ParamA", 20.0, -100.0, 100.0});
    config.parameters.push_back({"ParamB", -10.0, -100.0, 100.0});
    config.maxWeightIterations = 30;
    config.h = 3.0;
    config.priorVariance = 100.0;

    CLOPModel model(config);

    std::vector<CLOPModelSample> samples;
    constexpr double observationWeight = 512.0;

    for (int indexA = -10; indexA <= 10; ++indexA) {
        for (int indexB = -10; indexB <= 10; ++indexB) {
            const double valueA = static_cast<double>(indexA) * 10.0;
            const double valueB = static_cast<double>(indexB) * 10.0;
            const double distanceA = valueA - 30.0;
            const double distanceB = valueB + 20.0;
            const double quadratic = (distanceA * distanceA + distanceB * distanceB) / 4500.0;
            const double outcome = std::clamp(1.0 - quadratic, 0.0, 1.0);

            CLOPModelSample sample;
            sample.values = {valueA, valueB};
            sample.normalizedValues = model.normalizeValues(sample.values);
            sample.outcome = outcome;
            sample.observationWeight = observationWeight;
            samples.push_back(std::move(sample));
        }
    }

    REQUIRE_NOTHROW(model.updateDesignWeights(samples));

    for (const auto& sample : samples) {
        REQUIRE(std::isfinite(sample.designWeight));
        REQUIRE(sample.designWeight >= 0.0);
    }

    const auto estimated = model.computeEstimatedOptimum(samples);

    REQUIRE(estimated.size() == 2);
    REQUIRE(std::isfinite(estimated[0]));
    REQUIRE(std::isfinite(estimated[1]));
    REQUIRE(estimated[0] >= -100.0);
    REQUIRE(estimated[0] <= 100.0);
    REQUIRE(estimated[1] >= -100.0);
    REQUIRE(estimated[1] <= 100.0);
}
