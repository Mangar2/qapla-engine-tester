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
#pragma once

#include <cmath>
#include <utility>
#include <algorithm>

/**
 * @brief Approximates inverse error function using Winitzki's formula.
 */
inline double erfInv(double x) {
    constexpr double pi = 3.1415926535897;
    const double a = 8.0 * (pi - 3.0) / (3.0 * pi * (4.0 - pi));
    const double y = std::log(1.0 - x * x);
    const double z = 2.0 / (pi * a) + y / 2.0;

    double ret = std::sqrt(std::sqrt(z * z - y / a) - z);
    return x < 0.0 ? -ret : ret;
}

/**
 * @brief Approximates inverse cumulative normal distribution (probit).
 */
inline double phiInv(double p) {
    return std::sqrt(2.0) * erfInv(2.0 * p - 1.0);
}

/**
 * @brief Computes Elo and error margin using logistic model and cutechess-style estimation.
 */
inline std::pair<int, int> computeEloWithError(int wins, int losses, int draws) {
    const int total = wins + losses + draws;
    if (total < 0) return { 0, 0 };

    const double w = static_cast<double>(wins) / total;
    const double l = static_cast<double>(losses) / total;
    const double d = static_cast<double>(draws) / total;
    const double mu = w + 0.5 * d;

    const double p = std::clamp(mu, 1e-6, 1.0 - 1e-6);
    const double elo = -400.0 * std::log10(1.0 / p - 1.0);

    const double devW = w * std::pow(1.0 - mu, 2.0);
    const double devL = l * std::pow(0.0 - mu, 2.0);
    const double devD = d * std::pow(0.5 - mu, 2.0);
    const double stdev = std::sqrt(devW + devL + devD) / std::sqrt(total);

    const double muMin = mu + phiInv(0.025) * stdev;
    const double muMax = mu + phiInv(0.975) * stdev;

    const double eloMin = -400.0 * std::log10(1.0 / std::clamp(muMax, 1e-6, 1.0 - 1e-6) - 1.0);
    const double eloMax = -400.0 * std::log10(1.0 / std::clamp(muMin, 1e-6, 1.0 - 1e-6) - 1.0);

    const double error = std::abs(eloMax - eloMin) / 2.0;

    return { static_cast<int>(std::round(elo)), static_cast<int>(std::round(error)) };
}
