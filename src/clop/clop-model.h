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

#pragma once

#include "clop-types.h"

#include <random>
#include <vector>

namespace QaplaTester {

/**
 * @brief One CLOP observation used by the regression model.
 */
struct CLOPModelSample {
    std::vector<double> values;
    std::vector<double> normalizedValues;
    double outcome = 0.5;
    double observationWeight = 1.0;
    double designWeight = 1.0;
};

/**
 * @brief Regression and weighting core of the CLOP algorithm.
 */
class CLOPModel {
public:
    /**
     * @brief Constructs a model core for a specific CLOP configuration.
     * @param config CLOP settings containing parameter bounds and model hyperparameters.
     */
    explicit CLOPModel(const CLOPConfig& config);

    /**
     * @brief Maps normalized values in [-1, 1] to real parameter ranges.
     * @param normalizedValues Values in normalized domain.
     * @return Values in original parameter domain.
     */
    [[nodiscard]] std::vector<double> denormalizeValues(const std::vector<double>& normalizedValues) const;

    /**
     * @brief Maps real parameter values to normalized domain [-1, 1].
     * @param values Values in original parameter domain.
     * @return Values in normalized domain.
     */
    [[nodiscard]] std::vector<double> normalizeValues(const std::vector<double>& values) const;

    /**
     * @brief Updates CLOP design weights for all samples.
     * @param samples Samples to reweight in-place.
     * @return Converged weight density for subsequent Gibbs sampling.
     */
    [[nodiscard]] CLOPWeightDensity updateDesignWeights(std::vector<CLOPModelSample>& samples) const;

    /**
     * @brief Draws a new sample from the continuous CLOP weight density via Gibbs sampling.
     * @param density Converged weight density from updateDesignWeights.
     * @param startPoint Starting point in normalized [-1,1]^n domain.
     * @param rng Random engine for sampling.
     * @return New sample point in normalized domain.
     */
    [[nodiscard]] std::vector<double> sampleFromDensity(
        const CLOPWeightDensity& density,
        const std::vector<double>& startPoint,
        std::mt19937& rng) const;

    /**
     * @brief Computes CLOP estimated optimum from weighted samples.
     * @param samples Weighted model samples.
     * @return Estimated best parameter vector in original domain.
     */
    [[nodiscard]] std::vector<double> computeEstimatedOptimum(const std::vector<CLOPModelSample>& samples) const;

    /**
     * @brief Computes data-driven signal evidence by cross-validated model-vs-noise comparison.
     * @param samples Weighted model samples.
     * @param testConfig Signal test configuration from CLOPConfig.
     * @param seed Random seed for fold shuffling and permutations.
     * @return Signal evidence metrics.
     */
    [[nodiscard]] CLOPSignalEvidence computeSignalEvidence(
        const std::vector<CLOPModelSample>& samples,
        const CLOPSignalTestConfig& testConfig,
        uint32_t seed) const;

private:
    struct LogisticModel {
        std::vector<double> coefficients;
    };

    [[nodiscard]] double computeDeltaLogLoss(
        const std::vector<CLOPModelSample>& samples,
        size_t offset,
        const std::vector<size_t>& foldIds,
        uint32_t folds,
        const std::vector<double>* overriddenOutcomes) const;

    [[nodiscard]] LogisticModel fitQuadraticLogisticRegression(const std::vector<CLOPModelSample>& samples) const;
    [[nodiscard]] double fitLogisticMean(const std::vector<CLOPModelSample>& samples) const;
    [[nodiscard]] double confidenceDeviation(double meanLogit, const std::vector<CLOPModelSample>& samples) const;

    struct ConditionalQuadratic {
        double quadraticCoeff = 0.0;
        double linearCoeff = 0.0;
    };

    [[nodiscard]] ConditionalQuadratic extractConditionalQuadratic(
        const CLOPWeightDensity& density,
        const std::vector<double>& point,
        size_t dimension) const;

    [[nodiscard]] size_t featureCount() const;
    [[nodiscard]] std::vector<double> buildFeatureVector(const std::vector<double>& normalizedValues) const;
    [[nodiscard]] double evaluateQuadratic(const LogisticModel& model, const std::vector<double>& normalizedValues) const;

    CLOPConfig config_;
};

} // namespace QaplaTester
