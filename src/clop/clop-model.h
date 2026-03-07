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
     */
    void updateDesignWeights(std::vector<CLOPModelSample>& samples) const;

    /**
     * @brief Computes CLOP estimated optimum from weighted samples.
     * @param samples Weighted model samples.
     * @return Estimated best parameter vector in original domain.
     */
    [[nodiscard]] std::vector<double> computeEstimatedOptimum(const std::vector<CLOPModelSample>& samples) const;

private:
    struct LogisticModel {
        std::vector<double> coefficients;
    };

    [[nodiscard]] LogisticModel fitQuadraticLogisticRegression(const std::vector<CLOPModelSample>& samples) const;
    [[nodiscard]] double fitLogisticMean(const std::vector<CLOPModelSample>& samples) const;
    [[nodiscard]] double confidenceDeviation(double meanLogit, const std::vector<CLOPModelSample>& samples) const;

    [[nodiscard]] size_t featureCount() const;
    [[nodiscard]] std::vector<double> buildFeatureVector(const std::vector<double>& normalizedValues) const;
    [[nodiscard]] double evaluateQuadratic(const LogisticModel& model, const std::vector<double>& normalizedValues) const;

    CLOPConfig config_;
};

} // namespace QaplaTester
