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
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include "clop-model.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace QaplaTester {

namespace {

[[nodiscard]] double sigmoid(double value) {
    if (value >= 0.0) {
        const double expValue = std::exp(-value);
        return 1.0 / (1.0 + expValue);
    }

    const double expValue = std::exp(value);
    return expValue / (1.0 + expValue);
}

[[nodiscard]] double clampExpInput(double value) {
    return std::clamp(value, -40.0, 40.0);
}

} // namespace

CLOPModel::CLOPModel(const CLOPConfig& config)
    : config_(config) {
}

std::vector<double> CLOPModel::denormalizeValues(const std::vector<double>& normalizedValues) const {
    std::vector<double> values;
    values.reserve(normalizedValues.size());
    for (size_t index = 0; index < normalizedValues.size(); ++index) {
        const auto& parameter = config_.parameters[index];
        const double span = parameter.maxValue - parameter.minValue;
        const double mapped = (normalizedValues[index] + 1.0) * 0.5;
        values.push_back(parameter.minValue + std::clamp(mapped, 0.0, 1.0) * span);
    }
    return values;
}

std::vector<double> CLOPModel::normalizeValues(const std::vector<double>& values) const {
    std::vector<double> normalized;
    normalized.reserve(values.size());
    for (size_t index = 0; index < values.size(); ++index) {
        const auto& parameter = config_.parameters[index];
        const double span = parameter.maxValue - parameter.minValue;
        if (std::abs(span) <= std::numeric_limits<double>::epsilon()) {
            normalized.push_back(0.0);
            continue;
        }
        const double mapped = (values[index] - parameter.minValue) / span;
        normalized.push_back(std::clamp(mapped * 2.0 - 1.0, -1.0, 1.0));
    }
    return normalized;
}

void CLOPModel::updateDesignWeights(std::vector<CLOPModelSample>& samples) const {
    if (samples.empty()) {
        return;
    }

    double previousSum = 0.0;
    for (auto& sample : samples) {
        sample.designWeight = 1.0;
        previousSum += 1.0;
    }

    for (uint32_t iteration = 0; iteration < config_.maxWeightIterations; ++iteration) {
        auto model = fitQuadraticLogisticRegression(samples);
        const double meanLogit = fitLogisticMean(samples);
        const double sigma = std::max(confidenceDeviation(meanLogit, samples), 1e-8);

        double nextSum = 0.0;
        for (auto& sample : samples) {
            const double quadraticValue = evaluateQuadratic(model, sample.normalizedValues);
            const double exponent = clampExpInput((quadraticValue - meanLogit) / (config_.h * sigma));
            const double newWeight = std::exp(exponent);
            sample.designWeight = std::min(sample.designWeight, newWeight);
            nextSum += sample.designWeight;
        }

        if (nextSum > 0.99 * previousSum) {
            break;
        }
        previousSum = nextSum;
    }
}

std::vector<double> CLOPModel::computeEstimatedOptimum(const std::vector<CLOPModelSample>& samples) const {
    if (samples.empty()) {
        std::vector<double> midpointValues;
        midpointValues.reserve(config_.parameters.size());
        for (const auto& parameter : config_.parameters) {
            midpointValues.push_back((parameter.minValue + parameter.maxValue) * 0.5);
        }
        return midpointValues;
    }

    std::vector<double> weightedSum(config_.parameters.size(), 0.0);
    double totalWeight = 0.0;
    for (const auto& sample : samples) {
        const double effectiveWeight = sample.designWeight;
        totalWeight += effectiveWeight;
        for (size_t parameterIndex = 0; parameterIndex < weightedSum.size(); ++parameterIndex) {
            weightedSum[parameterIndex] += effectiveWeight * sample.values[parameterIndex];
        }
    }

    if (totalWeight <= std::numeric_limits<double>::epsilon()) {
        std::vector<double> midpointValues;
        midpointValues.reserve(config_.parameters.size());
        for (const auto& parameter : config_.parameters) {
            midpointValues.push_back((parameter.minValue + parameter.maxValue) * 0.5);
        }
        return midpointValues;
    }

    std::vector<double> estimated(weightedSum.size(), 0.0);
    for (size_t parameterIndex = 0; parameterIndex < weightedSum.size(); ++parameterIndex) {
        estimated[parameterIndex] = weightedSum[parameterIndex] / totalWeight;
    }

    return estimated;
}

CLOPModel::LogisticModel CLOPModel::fitQuadraticLogisticRegression(const std::vector<CLOPModelSample>& samples) const {
    LogisticModel model;
    model.coefficients.assign(featureCount(), 0.0);

    if (samples.empty()) {
        return model;
    }

    constexpr size_t maxIterations = 120;
    constexpr double learningRate = 0.03;

    const double totalEffectiveWeight = std::accumulate(
        samples.begin(),
        samples.end(),
        0.0,
        [](double sum, const CLOPModelSample& sampleEntry) {
            return sum + sampleEntry.designWeight * sampleEntry.observationWeight;
        });
    const double normalizationFactor = 1.0 / std::max(totalEffectiveWeight, 1.0);

    std::vector<double> gradient(model.coefficients.size(), 0.0);

    for (size_t iteration = 0; iteration < maxIterations; ++iteration) {
        std::fill(gradient.begin(), gradient.end(), 0.0);

        for (const auto& sample : samples) {
            const auto features = buildFeatureVector(sample.normalizedValues);
            const double linearValue = std::inner_product(
                model.coefficients.begin(),
                model.coefficients.end(),
                features.begin(),
                0.0);
            const double probability = sigmoid(linearValue);
            const double weightedError =
                sample.designWeight * sample.observationWeight * (sample.outcome - probability);

            for (size_t featureIndex = 0; featureIndex < features.size(); ++featureIndex) {
                gradient[featureIndex] += weightedError * features[featureIndex];
            }
        }

        for (size_t featureIndex = 0; featureIndex < gradient.size(); ++featureIndex) {
            gradient[featureIndex] -= model.coefficients[featureIndex] / config_.priorVariance;
            gradient[featureIndex] *= normalizationFactor;
            model.coefficients[featureIndex] += learningRate * gradient[featureIndex];
        }
    }

    return model;
}

double CLOPModel::fitLogisticMean(const std::vector<CLOPModelSample>& samples) const {
    if (samples.empty()) {
        return 0.0;
    }

    double intercept = 0.0;
    constexpr size_t maxIterations = 120;
    constexpr double learningRate = 0.05;

    const double totalEffectiveWeight = std::accumulate(
        samples.begin(),
        samples.end(),
        0.0,
        [](double sum, const CLOPModelSample& sampleEntry) {
            return sum + sampleEntry.designWeight * sampleEntry.observationWeight;
        });
    const double normalizationFactor = 1.0 / std::max(totalEffectiveWeight, 1.0);

    for (size_t iteration = 0; iteration < maxIterations; ++iteration) {
        const double probability = sigmoid(intercept);
        double gradient = -intercept / config_.priorVariance;

        for (const auto& sample : samples) {
            const double effectiveWeight = sample.designWeight * sample.observationWeight;
            gradient += effectiveWeight * (sample.outcome - probability);
        }

        gradient *= normalizationFactor;
        intercept += learningRate * gradient;
    }

    return intercept;
}

double CLOPModel::confidenceDeviation(double meanLogit, const std::vector<CLOPModelSample>& samples) const {
    const double probability = sigmoid(meanLogit);
    double precision = 1.0 / config_.priorVariance;

    for (const auto& sample : samples) {
        const double effectiveWeight = sample.designWeight * sample.observationWeight;
        precision += effectiveWeight * probability * (1.0 - probability);
    }

    return 1.0 / std::sqrt(std::max(precision, 1e-12));
}

size_t CLOPModel::featureCount() const {
    const size_t dimension = config_.parameters.size();
    const size_t linearTerms = dimension;
    const size_t diagonalTerms = dimension;
    const size_t crossTerms = dimension * (dimension - 1) / 2;
    return 1 + linearTerms + diagonalTerms + crossTerms;
}

std::vector<double> CLOPModel::buildFeatureVector(const std::vector<double>& normalizedValues) const {
    std::vector<double> featureVector;
    featureVector.reserve(featureCount());

    featureVector.push_back(1.0);
    featureVector.insert(featureVector.end(), normalizedValues.begin(), normalizedValues.end());

    for (const double value : normalizedValues) {
        featureVector.push_back(value * value);
    }

    for (size_t first = 0; first < normalizedValues.size(); ++first) {
        for (size_t second = first + 1; second < normalizedValues.size(); ++second) {
            featureVector.push_back(normalizedValues[first] * normalizedValues[second]);
        }
    }

    return featureVector;
}

double CLOPModel::evaluateQuadratic(
    const LogisticModel& model,
    const std::vector<double>& normalizedValues) const {

    const auto featureVector = buildFeatureVector(normalizedValues);
    return std::inner_product(
        model.coefficients.begin(),
        model.coefficients.end(),
        featureVector.begin(),
        0.0);
}

} // namespace QaplaTester
