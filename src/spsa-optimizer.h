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

#include "engine-config.h"
#include "pair-tournament.h"
#include "game-start-position.h"
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <random>

namespace QaplaTester {

class GameManagerPool;

/**
 * @brief Configuration for a single parameter to be optimized via SPSA
 */
struct SPSAParameterConfig {
    std::string name;           // UCI parameter name
    double defaultValue;        // Starting value
    double minValue;            // Minimum allowed value
    double maxValue;            // Maximum allowed value
    double c;                   // Step size for perturbation (c_i in the algorithm)
};

using SPSAParameterList = std::vector<SPSAParameterConfig>;

/**
 * @brief Configuration for SPSA optimization
 */
struct SPSAConfig {
    SPSAParameterList parameters;       // Parameters to optimize
    uint32_t maxActivePairs = 32;       // Number of unfinished tournament pairs
    double learningRate = 0.002;        // Global learning rate (r in the algorithm)
    uint32_t gamesPerPair = 8;          // Games per pairing
    uint32_t iterations = 1000;         // Maximum number of iterations
    std::string openingsFile;           // Path to openings file
    uint32_t openingsSeed = 0;          // Seed for opening selection
    bool swapColors = true;             // Whether to swap colors
};

/**
 * @brief Perturbation direction and associated tournament
 */
struct SPSAPerturbation {
    std::vector<int> deltas;                    // ±1 for each parameter
    std::vector<double> perturbedValues;        // Actual parameter values used
    std::shared_ptr<PairTournament> pairing;    // Associated tournament
    size_t iteration;                           // Iteration number
};

/**
 * @brief SPSA optimizer for tuning engine parameters
 * 
 * Implements the Simultaneous Perturbation Stochastic Approximation algorithm
 * for parallel parameter optimization of chess engines.
 */
class SPSAOptimizer {
public:
    SPSAOptimizer() = default;
    ~SPSAOptimizer();

    /**
     * @brief Create and initialize SPSA optimization
     * @param engine The engine configuration to optimize
     * @param config SPSA configuration including parameters and settings
     */
    void createSPSA(const EngineConfig& engine, const SPSAConfig& config);

    /**
     * @brief Schedule SPSA optimization tasks
     * @param concurrency Number of concurrent games
     * @param pool Game manager pool for scheduling
     */
    void scheduleSPSA(uint32_t concurrency, GameManagerPool& pool);

    /**
     * @brief Get current best parameter values
     * @return Vector of current parameter values
     */
    std::vector<double> getCurrentParameters() const;

    /**
     * @brief Get the number of completed iterations
     */
    size_t getCompletedIterations() const { return completedIterations_; }

    /**
     * @brief Print current optimization status
     */
    void printStatus(std::ostream& out) const;

    /**
     * @brief Get a specific perturbation by index
     * @param index Index of the perturbation
     * @return Optional containing the PairTournament pointer if index is valid, otherwise std::nullopt
     */
    std::optional<const PairTournament*> getPairTournament(size_t index) const {
        std::scoped_lock lock(stateMutex_);
        if (index < perturbations_.size() && perturbations_[index]) {
            return perturbations_[index]->pairing.get();
        }
        return std::nullopt;
    }

    /**
     * @brief Get the number of perturbations (active and finished)
     */
    size_t perturbationCount() const {
        std::scoped_lock lock(stateMutex_);
        return perturbations_.size();
    }

private:
    /**
     * @brief Callback when a pair tournament finishes
     * @param sender The finished tournament
     */
    void onPairFinished(PairTournament* sender);

    /**
     * @brief Create a new pair tournament with perturbed parameters
     * @return Shared pointer to the new perturbation
     */
    std::shared_ptr<SPSAPerturbation> createPairWithPerturbedParameters();

    /**
     * @brief Update parameters based on finished tournament results
     * @param perturbation The perturbation that finished
     */
    void updateParameters(const SPSAPerturbation& perturbation);

    /**
     * @brief Apply perturbations to create modified engine config
     * @param deltas Vector of ±1 perturbation directions
     * @return Modified engine configuration
     */
    EngineConfig createPerturbedEngineConfig(const std::vector<int>& deltas, 
                                              std::vector<double>& perturbedValues);

    /**
     * @brief Generate random perturbation deltas (±1)
     */
    std::vector<int> generatePerturbationDeltas();

    /**
     * @brief Log current parameter values
     * @param stage Description of the current stage (e.g., "Initial", "After iteration 5")
     */
    void logParameters(const std::string& stage) const;

    /**
     * @brief Worker thread function that creates new perturbations when needed
     */
    void workerThreadFunction();

    EngineConfig baseEngine_;
    SPSAConfig config_;
    std::vector<double> currentParameters_;     // Current best parameters (θ)
    std::shared_ptr<StartPositions> startPositions_;
    GameManagerPool* pool_ = nullptr;           // Pool reference for scheduling
    
    std::vector<std::shared_ptr<SPSAPerturbation>> perturbations_; // All perturbations
    size_t completedIterations_ = 0;
    size_t nextIteration_ = 0;
    uint32_t nextRound_ = 0;                    // Counter for unique round numbers
    std::atomic<size_t> activePerturbationCount_ = 0;  // Count of perturbations that are not finished
    
    mutable std::mutex stateMutex_;
    std::mt19937 rng_;
    bool initialized_ = false;
    bool scheduled_ = false;
    
    // Worker thread for creating new perturbations
    std::thread workerThread_;
    std::condition_variable workerCondition_;
    std::mutex workerMutex_;
    std::atomic<bool> stopWorker_ = false;
};

} // namespace QaplaTester
