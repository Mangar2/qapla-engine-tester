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
#include <numeric>
#include <random>

using namespace QaplaTester;

TEST_CASE("CLOPModel converges to interior optimum on smooth synthetic landscape", "[clop][model]") {
    CLOPConfig config;
    config.parameters.push_back({"Tunable", 0.0, 100.0});
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

    CLOPWeightDensity density;
    REQUIRE_NOTHROW(density = model.updateDesignWeights(samples));
    const auto estimated = model.computeEstimatedOptimum(samples);

    REQUIRE(estimated.size() == 1);
    REQUIRE(std::isfinite(estimated[0]));
    REQUIRE(estimated[0] >= 0.0);
    REQUIRE(estimated[0] <= 100.0);
    REQUIRE(estimated[0] > 55.0);
    REQUIRE(estimated[0] < 90.0);
}

TEST_CASE("CLOPModel remains finite with high observation weights", "[clop][model][stability]") {
    CLOPConfig config;
    config.parameters.push_back({"ParamA", -100.0, 100.0});
    config.parameters.push_back({"ParamB", -100.0, 100.0});
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

    CLOPWeightDensity density;
    REQUIRE_NOTHROW(density = model.updateDesignWeights(samples));

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

TEST_CASE("CLOP iterative loop converges on Log problem from paper", "[clop][model][convergence]") {
    CLOPConfig config;
    config.parameters.push_back({"x", -1.0, 1.0});
    config.maxWeightIterations = 25;
    config.h = 3.0;
    config.priorVariance = 100.0;

    CLOPModel model(config);
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> uniformDist(-1.0, 1.0);

    auto logFunction = [](double val) {
        const double logit = 2.0 * std::log(4.0 * val + 4.1) - 4.0 * val - 3.0;
        return 1.0 / (1.0 + std::exp(-logit));
    };

    std::vector<CLOPModelSample> samples;
    CLOPWeightDensity density;

    constexpr size_t warmupCount = 20;
    for (size_t idx = 0; idx < warmupCount; ++idx) {
        const double normalized = uniformDist(rng);
        const double realValue = normalized;
        const double winProb = logFunction(realValue);
        std::bernoulli_distribution trial(winProb);

        CLOPModelSample sample;
        sample.values = {realValue};
        sample.normalizedValues = {normalized};
        sample.outcome = trial(rng) ? 1.0 : 0.0;
        sample.observationWeight = 1.0;
        samples.push_back(std::move(sample));
    }

    constexpr size_t totalSamples = 800;
    for (size_t idx = warmupCount; idx < totalSamples; ++idx) {
        density = model.updateDesignWeights(samples);

        const auto estimated = model.computeEstimatedOptimum(samples);
        const auto normalizedEstimate = model.normalizeValues(estimated);
        const auto newPoint = model.sampleFromDensity(density, normalizedEstimate, rng);
        const auto realValues = model.denormalizeValues(newPoint);

        const double winProb = logFunction(realValues[0]);
        std::bernoulli_distribution trial(winProb);

        CLOPModelSample sample;
        sample.values = realValues;
        sample.normalizedValues = newPoint;
        sample.outcome = trial(rng) ? 1.0 : 0.0;
        sample.observationWeight = 1.0;
        samples.push_back(std::move(sample));
    }

    density = model.updateDesignWeights(samples);
    const auto finalEstimate = model.computeEstimatedOptimum(samples);

    REQUIRE(finalEstimate.size() == 1);
    REQUIRE(std::isfinite(finalEstimate[0]));
    REQUIRE(finalEstimate[0] > -0.6);
    REQUIRE(finalEstimate[0] < 0.2);
}

TEST_CASE("Gibbs sampling concentrates around high-weight region", "[clop][model][sampling]") {
    CLOPConfig config;
    config.parameters.push_back({"x", 0.0, 100.0});
    config.maxWeightIterations = 25;
    config.h = 3.0;
    config.priorVariance = 100.0;

    CLOPModel model(config);

    std::vector<CLOPModelSample> samples;
    constexpr double targetValue = 70.0;

    for (int idx = 0; idx <= 100; ++idx) {
        const double value = static_cast<double>(idx);
        const double distance = value - targetValue;
        const double outcome = 0.5 + 0.4 * std::exp(-(distance * distance) / 200.0);

        CLOPModelSample sample;
        sample.values = {value};
        sample.normalizedValues = model.normalizeValues(sample.values);
        sample.outcome = outcome;
        sample.observationWeight = 64.0;
        samples.push_back(std::move(sample));
    }

    const auto density = model.updateDesignWeights(samples);
    REQUIRE(density.valid);

    std::mt19937 rng(123);
    constexpr size_t sampleCount = 2000;
    double sumDenormalized = 0.0;
    size_t inTargetRegion = 0;

    const auto startPoint = model.normalizeValues({targetValue});
    for (size_t idx = 0; idx < sampleCount; ++idx) {
        const auto point = model.sampleFromDensity(density, startPoint, rng);
        const auto denorm = model.denormalizeValues(point);
        sumDenormalized += denorm[0];
        if (denorm[0] > 50.0 && denorm[0] < 90.0) {
            ++inTargetRegion;
        }
    }

    const double meanSampled = sumDenormalized / static_cast<double>(sampleCount);
    const double fractionInTarget = static_cast<double>(inTargetRegion) / static_cast<double>(sampleCount);

    REQUIRE(meanSampled > 55.0);
    REQUIRE(meanSampled < 85.0);
    REQUIRE(fractionInTarget > 0.5);
}

TEST_CASE("CLOP detects weak signal with noisy binary outcomes", "[clop][model][weaksignal]") {
    CLOPConfig config;
    config.parameters.push_back({"Strength", 0.0, 200.0});
    config.maxWeightIterations = 25;
    config.h = 3.0;
    config.priorVariance = 100.0;

    CLOPModel model(config);
    std::mt19937 rng(77);

    auto winRate = [](double value) {
        const double distance = value - 120.0;
        return 0.50 + 0.02 * std::exp(-(distance * distance) / 3200.0);
    };

    std::vector<CLOPModelSample> samples;
    CLOPWeightDensity density;

    constexpr size_t warmupCount = 30;
    std::uniform_real_distribution<double> uniformDist(-1.0, 1.0);
    for (size_t idx = 0; idx < warmupCount; ++idx) {
        const double normalized = uniformDist(rng);
        const auto realValues = model.denormalizeValues({normalized});

        constexpr size_t gamesPerSample = 16;
        const double prob = winRate(realValues[0]);
        std::binomial_distribution<int> binomDist(static_cast<int>(gamesPerSample), prob);
        const auto wins = static_cast<double>(binomDist(rng));

        CLOPModelSample sample;
        sample.values = realValues;
        sample.normalizedValues = {normalized};
        sample.outcome = wins / static_cast<double>(gamesPerSample);
        sample.observationWeight = static_cast<double>(gamesPerSample);
        samples.push_back(std::move(sample));
    }

    constexpr size_t totalSamples = 2000;
    for (size_t idx = warmupCount; idx < totalSamples; ++idx) {
        density = model.updateDesignWeights(samples);
        const auto estimated = model.computeEstimatedOptimum(samples);
        const auto normalizedEstimate = model.normalizeValues(estimated);
        const auto newPoint = model.sampleFromDensity(density, normalizedEstimate, rng);
        const auto realValues = model.denormalizeValues(newPoint);

        constexpr size_t gamesPerSample = 16;
        const double prob = winRate(realValues[0]);
        std::binomial_distribution<int> binomDist(static_cast<int>(gamesPerSample), prob);
        const auto wins = static_cast<double>(binomDist(rng));

        CLOPModelSample sample;
        sample.values = realValues;
        sample.normalizedValues = newPoint;
        sample.outcome = wins / static_cast<double>(gamesPerSample);
        sample.observationWeight = static_cast<double>(gamesPerSample);
        samples.push_back(std::move(sample));
    }

    density = model.updateDesignWeights(samples);
    const auto finalEstimate = model.computeEstimatedOptimum(samples);

    REQUIRE(finalEstimate.size() == 1);
    REQUIRE(std::isfinite(finalEstimate[0]));
    REQUIRE(finalEstimate[0] > 60.0);
    REQUIRE(finalEstimate[0] < 180.0);
}

TEST_CASE("CLOP iterative loop converges in 2D correlated landscape", "[clop][model][convergence][2d]") {
    CLOPConfig config;
    config.parameters.push_back({"ParamA", -1.0, 1.0});
    config.parameters.push_back({"ParamB", -1.0, 1.0});
    config.maxWeightIterations = 25;
    config.h = 3.0;
    config.priorVariance = 100.0;

    CLOPModel model(config);
    std::mt19937 rng(99);
    std::uniform_real_distribution<double> uniformDist(-1.0, 1.0);

    auto correlatedFunction = [](double valA, double valB) {
        auto quad = [](double val) {
            return -val * val * val * val + val * val * val - val * val / 3.0;
        };
        const double logit = 0.2 * (quad(10.0 * (valA + valB + 0.1)) + quad(valA - valB + 0.9)) + 0.2;
        return 1.0 / (1.0 + std::exp(-logit));
    };

    std::vector<CLOPModelSample> samples;
    CLOPWeightDensity density;

    constexpr size_t warmupCount = 40;
    for (size_t idx = 0; idx < warmupCount; ++idx) {
        const double normA = uniformDist(rng);
        const double normB = uniformDist(rng);
        const double winProb = correlatedFunction(normA, normB);
        std::bernoulli_distribution trial(std::clamp(winProb, 0.0, 1.0));

        CLOPModelSample sample;
        sample.values = {normA, normB};
        sample.normalizedValues = {normA, normB};
        sample.outcome = trial(rng) ? 1.0 : 0.0;
        sample.observationWeight = 1.0;
        samples.push_back(std::move(sample));
    }

    constexpr size_t totalSamples = 1500;
    for (size_t idx = warmupCount; idx < totalSamples; ++idx) {
        density = model.updateDesignWeights(samples);
        const auto estimated = model.computeEstimatedOptimum(samples);
        const auto newPoint = model.sampleFromDensity(density, estimated, rng);

        const double winProb = correlatedFunction(newPoint[0], newPoint[1]);
        std::bernoulli_distribution trial(std::clamp(winProb, 0.0, 1.0));

        CLOPModelSample sample;
        sample.values = newPoint;
        sample.normalizedValues = newPoint;
        sample.outcome = trial(rng) ? 1.0 : 0.0;
        sample.observationWeight = 1.0;
        samples.push_back(std::move(sample));
    }

    density = model.updateDesignWeights(samples);
    const auto finalEstimate = model.computeEstimatedOptimum(samples);

    REQUIRE(finalEstimate.size() == 2);
    REQUIRE(std::isfinite(finalEstimate[0]));
    REQUIRE(std::isfinite(finalEstimate[1]));

    const double estimatedWinRate = correlatedFunction(finalEstimate[0], finalEstimate[1]);
    const double midpointWinRate = correlatedFunction(0.0, 0.0);
    REQUIRE(estimatedWinRate > midpointWinRate);
}
