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

#include "spsa-optimizer.h"
#include "game-manager-pool.h"
#include "logger.h"
#include "opening-parser.h"
#include "app-error.h"
#include <iostream>
#include <iomanip>
#include <format>
#include <algorithm>

namespace QaplaTester {

void SPSAOptimizer::createSPSA(const EngineConfig& engine, const SPSAConfig& config) {
    if (config.parameters.empty()) {
        throw AppError::makeInvalidParameters("SPSA requires at least one parameter to optimize");
    }

    if (config.openingsFile.empty()) {
        throw AppError::makeInvalidParameters("SPSA requires an openings file");
    }

    baseEngine_ = engine;
    config_ = config;
    
    // Initialize current parameters to default values
    currentParameters_.clear();
    currentParameters_.reserve(config.parameters.size());
    for (const auto& param : config.parameters) {
        currentParameters_.push_back(param.defaultValue);
    }

    // Load openings
    if (!startPositions_) {
        startPositions_ = std::make_shared<StartPositions>();
    }

    OpeningParser parser;
    startPositions_->games = parser.parse(config.openingsFile);

    if (startPositions_->games.empty()) {
        throw AppError::makeInvalidParameters(
            "No valid openings found in file: " + config.openingsFile);
    }

    // Initialize random number generator
    rng_.seed(config.openingsSeed);
    
    initialized_ = true;
    scheduled_ = false;
    completedIterations_ = 0;
    nextIteration_ = 0;
    activePerturbations_.clear();

    // Create initial batch of perturbations (the actual tournament pairs)
    for (uint32_t i = 0; i < config.maxActivePairs && nextIteration_ < config.iterations; ++i) {
        auto perturbation = createPairWithPerturbedParameters();
        if (!perturbation) {
            break;
        }
    }

    Logger::reportLogger().log(
        std::format("SPSA initialized with {} parameters, {} initial pairs created",
                   config.parameters.size(), activePerturbations_.size()),
        TraceLevel::info);
    
    // Log initial parameter values
    logParameters("Initial");
}

void SPSAOptimizer::scheduleSPSA(uint32_t concurrency, GameManagerPool& pool) {
    if (!initialized_) {
        throw AppError::make("SPSA not initialized. Call createSPSA first.");
    }

    if (scheduled_) {
        throw AppError::make("SPSA already scheduled");
    }

    scheduled_ = true;
    pool_ = &pool;
    pool.setConcurrency(concurrency, true);

    // Schedule all existing perturbations
    for (const auto& perturbation : activePerturbations_) {
        if (perturbation && perturbation->pairing) {
            perturbation->pairing->schedule(perturbation->pairing, pool);
        }
    }

    Logger::reportLogger().log(
        std::format("SPSA scheduled {} pairs", activePerturbations_.size()),
        TraceLevel::info);
}

std::shared_ptr<SPSAPerturbation> SPSAOptimizer::createPairWithPerturbedParameters() {
    std::scoped_lock lock(stateMutex_);

    if (nextIteration_ >= config_.iterations) {
        return nullptr;
    }

    auto perturbation = std::make_shared<SPSAPerturbation>();
    perturbation->iteration = nextIteration_++;
    perturbation->deltas = generatePerturbationDeltas();

    // Create perturbed engine configurations
    auto enginePlus = createPerturbedEngineConfig(perturbation->deltas, perturbation->perturbedValues);
    enginePlus.setName(
        std::format("{}_iter{}_plus", baseEngine_.getName(), perturbation->iteration));
    
    // Create opposite perturbation for the opponent
    std::vector<int> minusDeltas = perturbation->deltas;
    for (auto& delta : minusDeltas) {
        delta = -delta;
    }
    std::vector<double> dummyValues;
    auto engineMinus = createPerturbedEngineConfig(minusDeltas, dummyValues);
    engineMinus.setName(
        std::format("{}_iter{}_minus", baseEngine_.getName(), perturbation->iteration));

    // Setup pair tournament configuration
    PairTournamentConfig ptc;
    ptc.games = config_.gamesPerPair;
    ptc.repeat = 1;
    ptc.swapColors = config_.swapColors;
    ptc.round = nextRound_++;
    ptc.gameNumberOffset = static_cast<uint32_t>(perturbation->iteration * config_.gamesPerPair);
    ptc.openings.start = static_cast<uint32_t>(rng_() % startPositions_->size());
    ptc.openings.policy = "default";
    ptc.seed = static_cast<uint32_t>(rng_());

    // Create the pair tournament
    perturbation->pairing = std::make_shared<PairTournament>();
    perturbation->pairing->initialize(enginePlus, engineMinus, ptc, startPositions_);
    
    // Register callback
    perturbation->pairing->setGameFinishedCallback([this](PairTournament* sender) {
        this->onPairFinished(sender);
    });

    activePerturbations_.push_back(perturbation);
    
    return perturbation;
}

void SPSAOptimizer::onPairFinished(PairTournament* sender) {
    // Check if the entire PairTournament is finished (not just a single game)
    if (!sender->isFinished()) {
        return;
    }

    std::shared_ptr<SPSAPerturbation> finishedPerturbation;
    uint32_t round = sender->getConfig().round;

    {
        std::scoped_lock lock(stateMutex_);
        
        // Find the finished perturbation by round number (direct indexing)
        if (round >= activePerturbations_.size()) {
            Logger::reportLogger().log(
                std::format("Warning: Invalid round number {} (size: {})", round, activePerturbations_.size()), 
                TraceLevel::warning);
            return;
        }

        finishedPerturbation = activePerturbations_[round];
        if (!finishedPerturbation || finishedPerturbation->pairing.get() != sender) {
            Logger::reportLogger().log(
                std::format("Warning: Round {} does not match expected pairing", round), 
                TraceLevel::warning);
            return;
        }

    }

    // Update parameters based on results
    updateParameters(*finishedPerturbation);

    // Print status
    {
        std::scoped_lock lock(stateMutex_);
        if (completedIterations_ % 10 == 0) {
            std::ostringstream oss;
            printStatus(oss);
            Logger::reportLogger().log(oss.str(), TraceLevel::info);
        }
    }

    // Schedule next perturbation if iterations remain
    if (nextIteration_ < config_.iterations && pool_) {
        auto newPerturbation = createPairWithPerturbedParameters();
        if (newPerturbation) {
            newPerturbation->pairing->schedule(newPerturbation->pairing, *pool_);
            Logger::reportLogger().log(
                std::format("Starting iteration {}/{}", 
                           newPerturbation->iteration + 1, config_.iterations),
                TraceLevel::info);
        }
    } else {
        std::scoped_lock lock(stateMutex_);
        // Check if all active perturbations are finished (all are nullptr)
        bool allFinished = std::all_of(activePerturbations_.begin(), activePerturbations_.end(),
            [](const auto& p) { return p == nullptr; });
        
        if (allFinished) {
            Logger::reportLogger().log("SPSA optimization complete!", TraceLevel::info);
            std::ostringstream oss;
            printStatus(oss);
            std::cout << oss.str() << std::endl;
        }
    }
}

void SPSAOptimizer::updateParameters(const SPSAPerturbation& perturbation) {
    auto result = perturbation.pairing->getResult();
    
    // Calculate gradient signal: wins - losses
    // In the normalized space, gradient g_φ_i ≈ (wins - losses) * Δ_i
    int wins = result.winsEngineA;
    int losses = result.winsEngineB;
    double gradient_signal = static_cast<double>(wins - losses);

    std::scoped_lock lock(stateMutex_);

    // Update each parameter using SPSA update rule
    // Δθ_i = r_i * c_i * g_φ_i, where g_φ_i = gradient_signal * Δ_i
    for (size_t i = 0; i < currentParameters_.size(); ++i) {
        double delta_i = static_cast<double>(perturbation.deltas[i]);
        double c_i = config_.parameters[i].c;
        double r_i = config_.learningRate;
        
        // SPSA gradient approximation in normalized space
        double g_phi_i = gradient_signal * delta_i;
        
        // Update in original parameter space: Δθ = r * c * g_φ
        double update = r_i * c_i * g_phi_i;
        currentParameters_[i] += update;
        
        // Clamp to bounds
        currentParameters_[i] = std::clamp(
            currentParameters_[i],
            config_.parameters[i].minValue,
            config_.parameters[i].maxValue
        );
    }

    completedIterations_++;
    
    // Log updated parameters
    logParameters(std::format("After iteration {}", completedIterations_));
}

EngineConfig SPSAOptimizer::createPerturbedEngineConfig(
    const std::vector<int>& deltas,
    std::vector<double>& perturbedValues) {
    
    EngineConfig config = baseEngine_;
    perturbedValues.clear();
    perturbedValues.reserve(config_.parameters.size());

    for (size_t i = 0; i < config_.parameters.size(); ++i) {
        const auto& paramConfig = config_.parameters[i];
        double baseValue = currentParameters_[i];
        
        // Apply perturbation: θ_i + c_i * Δ_i
        double perturbedValue = baseValue + paramConfig.c * deltas[i];
        perturbedValue = std::clamp(perturbedValue, paramConfig.minValue, paramConfig.maxValue);
        
        perturbedValues.push_back(perturbedValue);

        // Set UCI option value using the engine config's setOptionValue method
        config.setOptionValue(paramConfig.name, 
                             std::to_string(static_cast<int>(std::round(perturbedValue))));
    }

    return config;
}



std::vector<int> SPSAOptimizer::generatePerturbationDeltas() {
    std::vector<int> deltas;
    deltas.reserve(config_.parameters.size());
    
    std::uniform_int_distribution<int> dist(0, 1);
    
    for (size_t i = 0; i < config_.parameters.size(); ++i) {
        // Generate ±1 randomly
        deltas.push_back(dist(rng_) == 0 ? -1 : 1);
    }
    
    return deltas;
}

std::vector<double> SPSAOptimizer::getCurrentParameters() const {
    std::scoped_lock lock(stateMutex_);
    return currentParameters_;
}

void SPSAOptimizer::logParameters(const std::string& stage) const {
    std::ostringstream oss;
    oss << stage << " parameters: ";
    
    for (size_t i = 0; i < config_.parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << config_.parameters[i].name << "=" 
            << std::fixed << std::setprecision(2) << currentParameters_[i];
    }
    
    Logger::reportLogger().log(oss.str(), TraceLevel::error);
}

void SPSAOptimizer::printStatus(std::ostream& out) const {
    size_t activeCount = std::count_if(activePerturbations_.begin(), activePerturbations_.end(),
        [](const auto& p) { return p != nullptr; });
    
    out << "\n=== SPSA Optimization Status ===\n";
    out << "Completed iterations: " << completedIterations_ 
        << " / " << config_.iterations << "\n";
    out << "Active pairs: " << activeCount << "\n";
    out << "\nCurrent best parameters:\n";
    
    for (size_t i = 0; i < config_.parameters.size(); ++i) {
        const auto& param = config_.parameters[i];
        out << std::setw(20) << std::left << param.name << ": "
            << std::setw(10) << std::right << std::fixed << std::setprecision(2)
            << currentParameters_[i]
            << " (range: " << param.minValue << " - " << param.maxValue << ")\n";
    }
    out << "================================\n";
}

} // namespace QaplaTester
