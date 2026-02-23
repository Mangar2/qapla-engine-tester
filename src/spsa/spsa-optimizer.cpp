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

#include "../opening/opening-parser.h"

#include "../game-manager/game-manager-pool.h"
#include "../base-elements/logger.h"
#include "../base-elements/app-error.h"

#include <format>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace QaplaTester {

SPSAOptimizer::~SPSAOptimizer() {
    // Signal worker to stop
    stopWorker_ = true;
    workerCondition_.notify_one();
    
    // Wait for worker thread to finish
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

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
    parameterHistory_.clear();
    parameterHistory_.resize(config.parameters.size());
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
    activePerturbationCount_ = 0;
    perturbations_.clear();

    // Create initial batch of perturbations (the actual tournament pairs)
    for (uint32_t i = 0; i < config.maxActivePairs && nextIteration_ < config.iterations; ++i) {
        auto perturbation = createPairWithPerturbedParameters();
        if (!perturbation) {
            break;
        }
    }

    Logger::reportLogger().logStatus(
        std::format("SPSA initialized with {} parameters and {} initial pairs",
            config.parameters.size(), perturbations_.size()),
        "spsa",
        TraceLevel::info);
    logStatusTables("initial", TraceLevel::result);
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
    for (const auto& perturbation : perturbations_) {
        if (perturbation && perturbation->pairing) {
            perturbation->pairing->schedule(perturbation->pairing, pool);
        }
    }

    Logger::reportLogger().logStatus(
        std::format("SPSA scheduled {} active pairs", perturbations_.size()),
        "spsa",
        TraceLevel::info);
    
    // Start worker thread only if not already running
    if (!workerThread_.joinable()) {
        stopWorker_ = false;
        workerThread_ = std::thread(&SPSAOptimizer::workerThreadFunction, this);
    }
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
    perturbation->pairing->setVerbose(false);
    
    // Register callback
    perturbation->pairing->setGameFinishedCallback([this](PairTournament* sender) {
        this->onPairFinished(sender);
    });

    perturbations_.push_back(perturbation);
    ++activePerturbationCount_;

    Logger::reportLogger().logStatus(
        std::format("SPSA queued iteration {}/{} (round {})",
            perturbation->iteration + 1,
            config_.iterations,
            ptc.round),
        "spsa",
        TraceLevel::info);
    
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
        if (round >= perturbations_.size()) {
            Logger::reportLogger().logStatus(
                std::format("Warning: Invalid round number {} (size: {})", round, perturbations_.size()), 
                "spsa",
                TraceLevel::warning);
            return;
        }

        finishedPerturbation = perturbations_[round];
        if (!finishedPerturbation || finishedPerturbation->pairing.get() != sender) {
            Logger::reportLogger().logStatus(
                std::format("Warning: Round {} does not match expected pairing", round), 
                "spsa",
                TraceLevel::warning);
            return;
        }

    }

    // Update parameters based on results
    updateParameters(*finishedPerturbation);
    --activePerturbationCount_;

    bool logIntermediateStatus = false;
    {
        std::scoped_lock lock(stateMutex_);
        logIntermediateStatus = config_.outcomeInterval > 0 &&
            completedIterations_ % config_.outcomeInterval == 0;
    }
    if (logIntermediateStatus) {
        logStatusTables("progress", TraceLevel::result);
    }

    // Notify worker thread to create new perturbations if needed
    workerCondition_.notify_one();
    
    // Check if optimization is complete
    bool optimizationComplete = false;
    {
        std::scoped_lock lock(stateMutex_);
        optimizationComplete = activePerturbationCount_ == 0 && nextIteration_ >= config_.iterations;
    }
    if (optimizationComplete) {
        Logger::reportLogger().logStatus("SPSA optimization complete.", "spsa", TraceLevel::result);
        logStatusTables("final", TraceLevel::result);
    }
}

void SPSAOptimizer::updateParameters(const SPSAPerturbation& perturbation) {
    auto result = perturbation.pairing->getResult();
    
    // Calculate gradient signal: wins - losses
    // In normalized space, gradient g_phi_i ~= (wins - losses) * delta_i
    int wins = result.winsEngineA;
    int losses = result.winsEngineB;
    auto gradient_signal = static_cast<double>(wins - losses);

    std::scoped_lock lock(stateMutex_);

    // Update each parameter using SPSA update rule
    // delta_theta_i = r_i * c_i * g_phi_i, where g_phi_i = gradient_signal * delta_i
    for (size_t i = 0; i < currentParameters_.size(); ++i) {
        auto delta_i = static_cast<double>(perturbation.deltas[i]);
        auto c_i = config_.parameters[i].c;
        auto r_i = config_.learningRate;
        
        // SPSA gradient approximation in normalized space
        auto g_phi_i = gradient_signal * delta_i;
        
        // Update in original parameter space: delta_theta = r * c * g_phi
        auto update = r_i * c_i * g_phi_i;
        currentParameters_[i] += update;
        
        // Clamp to bounds
        currentParameters_[i] = std::clamp(
            currentParameters_[i],
            config_.parameters[i].minValue,
            config_.parameters[i].maxValue
        );

        // Store history for standard deviation calculation
        parameterHistory_[i].push_back(currentParameters_[i]);
    }

    completedIterations_++;
}

EngineConfig SPSAOptimizer::createPerturbedEngineConfig(
    const std::vector<int>& deltas,
    std::vector<double>& perturbedValues) {
    
    EngineConfig config = baseEngine_;
    perturbedValues.clear();
    perturbedValues.reserve(config_.parameters.size());

    for (size_t i = 0; i < config_.parameters.size(); ++i) {
        const auto& paramConfig = config_.parameters[i];
        auto baseValue = currentParameters_[i];
        
        // Apply perturbation: theta_i + c_i * delta_i
        auto perturbedValue = baseValue + paramConfig.c * deltas[i];
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
        // Generate +/-1 randomly
        deltas.push_back(dist(rng_) == 0 ? -1 : 1);
    }
    
    return deltas;
}

std::vector<double> SPSAOptimizer::getCurrentParameters() const {
    std::scoped_lock lock(stateMutex_);
    return currentParameters_;
}

double SPSAOptimizer::calculateStdDev(size_t paramIndex, size_t lastN) const {
    if (paramIndex >= parameterHistory_.size()) {
        return 0.0;
    }

    const auto& history = parameterHistory_[paramIndex];
    if (history.empty()) {
        return 0.0;
    }

    const size_t count = std::min(history.size(), lastN);
    const auto startIt = history.end() - static_cast<long long>(count);
    
    double sum = std::accumulate(startIt, history.end(), 0.0);
    double mean = sum / static_cast<double>(count);
    
    double sq_sum = std::accumulate(startIt, history.end(), 0.0, 
        [mean](double acc, double val) {
            return acc + (val - mean) * (val - mean);
        });
    
    return std::sqrt(sq_sum / static_cast<double>(count));
}

double SPSAOptimizer::calculateRelativeStdDev(size_t paramIndex, size_t lastN) const {
    if (paramIndex >= config_.parameters.size()) {
        return 0.0;
    }

    const auto standardDeviation = calculateStdDev(paramIndex, lastN);
    const auto referenceValue = std::abs(config_.parameters[paramIndex].defaultValue);
    if (referenceValue <= std::numeric_limits<double>::epsilon()) {
        return 0.0;
    }

    return standardDeviation / referenceValue;
}

TableData SPSAOptimizer::getProgressTable() const {
    std::scoped_lock lock(stateMutex_);

    TableData table;
    table.columnWidths = { 24, 12 };
    table.headers = { "Metric", "Value" };
    table.body.push_back({ "CompletedIterations", static_cast<std::uint64_t>(completedIterations_) });
    table.body.push_back({ "TotalIterations", static_cast<std::uint64_t>(config_.iterations) });
    table.body.push_back({ "ActivePairs", static_cast<std::uint64_t>(activePerturbationCount_.load()) });
    table.body.push_back({ "MaxActivePairs", static_cast<std::uint64_t>(config_.maxActivePairs) });

    return table;
}

void SPSAOptimizer::logStatusTables(std::string_view stage, TraceLevel level) const {
    size_t completedIterations = 0;
    uint32_t totalIterations = 0;
    size_t activePairs = 0;
    {
        std::scoped_lock lock(stateMutex_);
        completedIterations = completedIterations_;
        totalIterations = config_.iterations;
        activePairs = activePerturbationCount_.load();
    }

    Logger::reportLogger().logStatus(
        std::format(
            "SPSA {} status: {}/{} iterations, {} active pairs",
            stage,
            completedIterations,
            totalIterations,
            activePairs),
        "spsa",
        level);
    Logger::reportLogger().logTable("spsaProgress", getProgressTable(), level);
    Logger::reportLogger().logTable("spsaStatus", getStatusTable(), level);
}

void SPSAOptimizer::workerThreadFunction() {
    while (!stopWorker_) {
        std::unique_lock lock(workerMutex_);
        
        // Wait for notification or stop signal
        workerCondition_.wait(lock, [this] { 
            return stopWorker_ || 
                   (activePerturbationCount_ < config_.maxActivePairs && 
                    nextIteration_ < config_.iterations);
        });
        
        if (stopWorker_) {
            break;
        }
        
        lock.unlock();
        
        // Create new perturbations while there's room and iterations remain
        while (activePerturbationCount_ < config_.maxActivePairs && 
               nextIteration_ < config_.iterations && 
               pool_ != nullptr) {
            auto newPerturbation = createPairWithPerturbedParameters();
            if (!newPerturbation) {
                break;
            }
            
            newPerturbation->pairing->schedule(newPerturbation->pairing, *pool_);
            Logger::reportLogger().logStatus(
                std::format("Starting iteration {}/{}", 
                           newPerturbation->iteration + 1, config_.iterations),
                "spsa",
                TraceLevel::info);
        }
    }
}

void SPSAOptimizer::printStatus(std::ostream& out) const {
    out << TableFormat::toText(getProgressTable()) << "\n";
    out << TableFormat::toText(getStatusTable()) << "\n";
}

TableData SPSAOptimizer::getStatusTable() const {
    std::scoped_lock lock(stateMutex_);

    TableData table;
    table.columnWidths = { 20, 10, 10, 10, 10, 10, 10 };
    table.headers = { "Parameter", "Initial", "Value", "RelStd2k%", "RelStd5k%", "Min", "Max" };

    for (size_t parameterIndex = 0; parameterIndex < config_.parameters.size(); ++parameterIndex) {
        const auto& parameter = config_.parameters[parameterIndex];
        const auto standardDeviation2k = calculateRelativeStdDev(parameterIndex, 2000) * 100.0;
        const auto standardDeviation5k = calculateRelativeStdDev(parameterIndex, 5000) * 100.0;

        table.body.push_back({
            parameter.name,
            parameter.defaultValue,
            currentParameters_[parameterIndex],
            standardDeviation2k,
            standardDeviation5k,
            parameter.minValue,
            parameter.maxValue
        });
    }

    return table;
}

} // namespace QaplaTester
