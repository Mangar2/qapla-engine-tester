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

#include <cstdint>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief Configuration for one parameter tuned by CLOP.
 */
struct CLOPParameterConfig {
    std::string name;
    double minValue;
    double maxValue;
};

using CLOPParameterList = std::vector<CLOPParameterConfig>;

/**
 * @brief Thresholds used by the runtime CLOP indicator evaluation.
 */
struct CLOPIndicatorThresholds {
    double warmupProgressPercent = 15.0;
    double explorationEffectiveShareMin = 0.9;
    double searchingStepPercentMin = 0.2;
    double confidenceHighEffectiveShareMax = 0.75;
    double confidenceHighStepPercentMax = 0.1;
    double confidenceHighEffectivePerFeatureMin = 15.0;
    double confidenceMediumEffectiveShareMax = 0.85;
    double confidenceMediumStepPercentMax = 0.2;
    double confidenceMediumEffectivePerFeatureMin = 10.0;
};

/**
 * @brief Configuration for CLOP optimization execution.
 */
struct CLOPConfig {
    CLOPParameterList parameters;
    CLOPIndicatorThresholds indicatorThresholds;
    bool trace = false;
    uint32_t maxActivePairs = 32;
    uint32_t samples = 100;
    uint32_t gamesPerSample = 8;
    uint32_t warmupSamples = 8;
    uint32_t outcomeInterval = 10;
    uint32_t maxWeightIterations = 25;
    uint32_t openingsSeed = 0;
    double h = 3.0;
    double priorVariance = 100.0;
    std::string openingsFile;
};

} // namespace QaplaTester
