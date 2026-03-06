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

#include "clop-optimizer.h"

#include "../opening/opening-parser.h"
#include "../game-manager/game-manager-pool.h"
#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
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

CLOPOptimizer::~CLOPOptimizer() {
    stop();
}

void CLOPOptimizer::createCLOP(const EngineConfig& engine, const CLOPConfig& config) {
    if (config.parameters.empty()) {
        throw AppError::makeInvalidParameters("CLOP requires at least one parameter to optimize.");
    }
    if (config.samples == 0U) {
        throw AppError::makeInvalidParameters("CLOP requires samples > 0.");
    }
    if (config.gamesPerSample == 0U) {
        throw AppError::makeInvalidParameters("CLOP requires gamespersample > 0.");
    }
    if (config.openingsFile.empty()) {
        throw AppError::makeInvalidParameters("CLOP requires an openings file.");
    }

    baseEngine_ = engine;
    baselineEngine_ = engine;
    config_ = config;
    rng_.seed(config.openingsSeed);

    estimatedParameters_.clear();
    estimatedParameters_.reserve(config.parameters.size());
    for (const auto& parameter : config.parameters) {
        estimatedParameters_.push_back(parameter.defaultValue);
    }

    startPositions_ = std::make_shared<StartPositions>();
    OpeningParser parser;
    startPositions_->games = parser.parse(config.openingsFile);
    if (startPositions_->games.empty()) {
        throw AppError::makeInvalidParameters(
            std::format("No valid openings found in file: {}", config.openingsFile));
    }

    {
        std::scoped_lock lock(stateMutex_);
        samples_.clear();
        pendingResults_.clear();
        activeSamples_.clear();
        completedSamples_ = 0;
        lastLoggedCompletedSamples_ = 0;
        modelGeneration_ = 0;
        nextSampleIndex_ = 0;
        nextRound_ = 0;
        recomputeRunning_ = false;
        initialized_ = true;
        scheduled_ = false;
        finished_ = false;
        lastRecomputeAt_ = std::chrono::steady_clock::time_point{};
    }

    Logger::reportLogger().logStatus(
        std::format(
            "CLOP initialized with {} parameters, {} target samples and H={}",
            config.parameters.size(),
            config.samples,
            config.h),
        "clop",
        TraceLevel::result);
}

void CLOPOptimizer::scheduleCLOP(uint32_t concurrency, GameManagerPool& pool) {
    if (!initialized_) {
        throw AppError::make("CLOP not initialized. Call createCLOP first.");
    }
    if (scheduled_) {
        throw AppError::make("CLOP already scheduled.");
    }

    pool_ = &pool;
    pool_->setConcurrency(concurrency, true);
    scheduled_ = true;
    stopWorker_ = false;

    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    if (recomputeThread_.joinable()) {
        recomputeThread_.join();
    }

    schedulerThread_ = std::thread(&CLOPOptimizer::schedulerThreadFunction, this);
    recomputeThread_ = std::thread(&CLOPOptimizer::recomputeThreadFunction, this);
}

void CLOPOptimizer::waitUntilFinished() {
    {
        std::unique_lock lock(stateMutex_);
        stateCondition_.wait(lock, [this]() {
            return finished_ || stopWorker_;
        });
    }
    stop();
}

void CLOPOptimizer::stop() {
    stopWorker_ = true;
    stateCondition_.notify_all();
    recomputeCondition_.notify_all();

    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    if (recomputeThread_.joinable()) {
        recomputeThread_.join();
    }
}

std::vector<double> CLOPOptimizer::getEstimatedParameters() const {
    std::scoped_lock lock(stateMutex_);
    return estimatedParameters_;
}

size_t CLOPOptimizer::getCompletedSamples() const {
    std::scoped_lock lock(stateMutex_);
    return completedSamples_;
}

void CLOPOptimizer::updateFinishedStateLocked() {
    const bool enoughSamples = completedSamples_ >= config_.samples;
    const bool noActivePairs = activeSamples_.empty();
    const bool noPendingResults = pendingResults_.empty();
    if (enoughSamples && noActivePairs && noPendingResults && !recomputeRunning_) {
        finished_ = true;
    }
}

void CLOPOptimizer::schedulerThreadFunction() {
    while (!stopWorker_) {
        std::unique_lock lock(stateMutex_);
        updateFinishedStateLocked();
        if (finished_) {
            stateCondition_.notify_all();
            break;
        }

        const auto activeCount = activeSamples_.size();
        const auto totalScheduled = completedSamples_ + activeCount;
        if (activeCount >= config_.maxActivePairs || totalScheduled >= config_.samples) {
            stateCondition_.wait_for(lock, std::chrono::milliseconds(25));
            continue;
        }
        lock.unlock();

        while (!stopWorker_) {
            bool canScheduleMore = false;
            {
                std::scoped_lock stateLock(stateMutex_);
                const auto localActiveCount = activeSamples_.size();
                const auto localTotalScheduled = completedSamples_ + localActiveCount;
                canScheduleMore = localActiveCount < config_.maxActivePairs &&
                    localTotalScheduled < config_.samples;
            }

            if (!canScheduleMore) {
                break;
            }

            scheduleNextSample();
        }
    }
}

void CLOPOptimizer::recomputeThreadFunction() {
    while (!stopWorker_) {
        std::vector<CLOPSample> modelSnapshot;
        std::vector<CLOPSample> pendingBatch;
        bool reachedSampleLimit = false;

        {
            std::unique_lock lock(stateMutex_);
            recomputeCondition_.wait_for(lock, std::chrono::milliseconds(200), [this]() {
                return stopWorker_ || !pendingResults_.empty();
            });

            if (stopWorker_) {
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            if (!pendingResults_.empty() &&
                lastRecomputeAt_ != std::chrono::steady_clock::time_point{} &&
                now - lastRecomputeAt_ < minRecomputeInterval_)
            {
                continue;
            }

            if (pendingResults_.empty()) {
                continue;
            }

            recomputeRunning_ = true;
            modelSnapshot = samples_;
            pendingBatch.swap(pendingResults_);
            reachedSampleLimit = completedSamples_ >= config_.samples;
            lock.unlock();
        }

        modelSnapshot.insert(modelSnapshot.end(), pendingBatch.begin(), pendingBatch.end());
        updateDesignWeights(modelSnapshot);
        auto nextEstimatedParameters = computeEstimatedOptimum(modelSnapshot);

        bool shouldLog = false;
        bool isFinishedNow = false;
        {
            std::scoped_lock lock(stateMutex_);
            samples_ = std::move(modelSnapshot);
            estimatedParameters_ = std::move(nextEstimatedParameters);
            ++modelGeneration_;
            recomputeRunning_ = false;
            lastRecomputeAt_ = std::chrono::steady_clock::now();

            if (config_.outcomeInterval != 0U) {
                const size_t previousBucket = lastLoggedCompletedSamples_ / config_.outcomeInterval;
                const size_t currentBucket = completedSamples_ / config_.outcomeInterval;
                shouldLog = currentBucket > previousBucket;
                if (shouldLog) {
                    lastLoggedCompletedSamples_ = completedSamples_;
                }
            }
            if (reachedSampleLimit && pendingResults_.empty() && activeSamples_.empty()) {
                finished_ = true;
            }
            isFinishedNow = finished_;
        }

        if (shouldLog || isFinishedNow) {
            Logger::reportLogger().logTable("clopStatus", getStatusTable(), TraceLevel::result);
        }

        stateCondition_.notify_all();
    }
}

void CLOPOptimizer::scheduleNextSample() {
    if (pool_ == nullptr) {
        return;
    }

    std::vector<CLOPSample> modeledSamples;
    std::vector<double> estimatedParameters;
    std::vector<double> normalizedValues;
    std::vector<double> parameterValues;
    size_t sampleIndex = 0;
    size_t scheduledCount = 0;
    uint64_t modelGeneration = 0;

    {
        std::scoped_lock lock(stateMutex_);
        const auto totalScheduled = completedSamples_ + activeSamples_.size();
        if (totalScheduled >= config_.samples) {
            return;
        }

        modeledSamples = samples_;
        estimatedParameters = estimatedParameters_;
        modelGeneration = modelGeneration_;
        sampleIndex = nextSampleIndex_;
        ++nextSampleIndex_;
        scheduledCount = totalScheduled;
    }

    normalizedValues = createNormalizedSample(modeledSamples, estimatedParameters, scheduledCount);
    parameterValues = denormalizeValues(normalizedValues);

    auto challengerEngine = createConfiguredEngine(parameterValues);
    challengerEngine.setName(std::format("{}_clop_sample{}", baseEngine_.getName(), sampleIndex));

    PairTournamentConfig pairConfig;
    pairConfig.games = config_.gamesPerSample;
    pairConfig.repeat = 1;
    pairConfig.swapColors = true;
    pairConfig.round = nextRound_++;
    pairConfig.gameNumberOffset = static_cast<uint32_t>(sampleIndex * config_.gamesPerSample);
    pairConfig.openings.start = static_cast<uint32_t>(rng_() % startPositions_->size());
    pairConfig.openings.policy = "default";
    pairConfig.seed = static_cast<uint32_t>(rng_());

    auto pair = std::make_shared<PairTournament>();
    pair->initialize(challengerEngine, baselineEngine_, pairConfig, startPositions_);
    pair->setVerbose(false);
    pair->setGameFinishedCallback([this](PairTournament* sender) {
        onPairFinished(sender);
    });

    {
        std::scoped_lock lock(stateMutex_);
        CLOPSample active;
        active.values = std::move(parameterValues);
        active.normalizedValues = std::move(normalizedValues);
        active.index = sampleIndex;
        active.generation = modelGeneration;
        active.pairing = pair;
        activeSamples_.push_back(std::move(active));
    }

    pair->schedule(pair, *pool_);
}

void CLOPOptimizer::onPairFinished(PairTournament* sender) {
    if (sender == nullptr || !sender->isFinished()) {
        return;
    }

    std::optional<CLOPSample> finishedSample;
    {
        std::scoped_lock lock(stateMutex_);
        const auto iterator = std::find_if(activeSamples_.begin(), activeSamples_.end(),
            [sender](const CLOPSample& sample) {
                return sample.pairing != nullptr && sample.pairing.get() == sender;
            });

        if (iterator == activeSamples_.end()) {
            return;
        }

        finishedSample = *iterator;
        activeSamples_.erase(iterator);
    }

    if (!finishedSample.has_value() || finishedSample->pairing == nullptr) {
        return;
    }

    const auto result = finishedSample->pairing->getResult();
    const auto totalGames = static_cast<double>(std::max(1, result.total()));
    finishedSample->outcome =
        (static_cast<double>(result.winsEngineA) + 0.5 * static_cast<double>(result.draws)) / totalGames;
    finishedSample->observationWeight = totalGames;
    finishedSample->pairing.reset();

    {
        std::scoped_lock lock(stateMutex_);
        pendingResults_.push_back(std::move(*finishedSample));
        ++completedSamples_;
        updateFinishedStateLocked();
    }

    recomputeCondition_.notify_one();
    stateCondition_.notify_all();
}

std::vector<double> CLOPOptimizer::createNormalizedSample(
    const std::vector<CLOPSample>& modeledSamples,
    const std::vector<double>& estimatedParameters,
    size_t scheduledCount) {

    const size_t parameterCount = config_.parameters.size();
    std::vector<double> sample(parameterCount, 0.0);

    if (modeledSamples.size() < config_.warmupSamples) {
        std::uniform_real_distribution<double> randomDist(-1.0, 1.0);
        for (size_t index = 0; index < parameterCount; ++index) {
            sample[index] = randomDist(rng_);
        }
        return sample;
    }

    const double totalWeight = std::accumulate(
        modeledSamples.begin(),
        modeledSamples.end(),
        0.0,
        [](double sum, const CLOPSample& sampleEntry) {
            return sum + std::max(sampleEntry.designWeight, 0.0);
        });

    if (totalWeight <= std::numeric_limits<double>::epsilon()) {
        return normalizeValues(estimatedParameters);
    }

    std::uniform_real_distribution<double> selectDist(0.0, totalWeight);
    const double target = selectDist(rng_);
    double cumulative = 0.0;
    const CLOPSample* selected = nullptr;
    for (const auto& sampleEntry : modeledSamples) {
        cumulative += std::max(sampleEntry.designWeight, 0.0);
        if (cumulative >= target) {
            selected = &sampleEntry;
            break;
        }
    }
    if (selected == nullptr) {
        selected = &modeledSamples.back();
    }

    sample = selected->normalizedValues;
    const double sigma = 0.6 / std::sqrt(static_cast<double>(scheduledCount) + 1.0);
    std::normal_distribution<double> noiseDist(0.0, sigma);
    for (size_t index = 0; index < parameterCount; ++index) {
        sample[index] = std::clamp(sample[index] + noiseDist(rng_), -1.0, 1.0);
    }
    return sample;
}

std::vector<double> CLOPOptimizer::denormalizeValues(const std::vector<double>& normalizedValues) const {
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

std::vector<double> CLOPOptimizer::normalizeValues(const std::vector<double>& values) const {
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

EngineConfig CLOPOptimizer::createConfiguredEngine(const std::vector<double>& values) const {
    EngineConfig configuredEngine = baseEngine_;
    for (size_t index = 0; index < values.size(); ++index) {
        const auto roundedValue = static_cast<int>(std::lround(values[index]));
        configuredEngine.setOptionValue(config_.parameters[index].name, std::to_string(roundedValue));
    }
    return configuredEngine;
}

CLOPOptimizer::LogisticModel CLOPOptimizer::fitQuadraticLogisticRegression(const std::vector<CLOPSample>& samples) const {
    LogisticModel model;
    model.coefficients.assign(featureCount(), 0.0);

    if (samples.empty()) {
        return model;
    }

    constexpr size_t maxIterations = 120;
    constexpr double learningRate = 0.03;

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
            model.coefficients[featureIndex] += learningRate * gradient[featureIndex];
        }
    }

    return model;
}

double CLOPOptimizer::fitLogisticMean(const std::vector<CLOPSample>& samples) const {
    if (samples.empty()) {
        return 0.0;
    }

    double intercept = 0.0;
    constexpr size_t maxIterations = 120;
    constexpr double learningRate = 0.05;

    for (size_t iteration = 0; iteration < maxIterations; ++iteration) {
        const double probability = sigmoid(intercept);
        double gradient = -intercept / config_.priorVariance;

        for (const auto& sample : samples) {
            const double effectiveWeight = sample.designWeight * sample.observationWeight;
            gradient += effectiveWeight * (sample.outcome - probability);
        }

        intercept += learningRate * gradient;
    }

    return intercept;
}

double CLOPOptimizer::confidenceDeviation(double meanLogit, const std::vector<CLOPSample>& samples) const {
    const double probability = sigmoid(meanLogit);
    double precision = 1.0 / config_.priorVariance;

    for (const auto& sample : samples) {
        const double effectiveWeight = sample.designWeight * sample.observationWeight;
        precision += effectiveWeight * probability * (1.0 - probability);
    }

    return 1.0 / std::sqrt(std::max(precision, 1e-12));
}

void CLOPOptimizer::updateDesignWeights(std::vector<CLOPSample>& samples) const {
    if (samples.empty()) {
        return;
    }

    double previousSum = 0.0;
    for (auto& sample : samples) {
        sample.designWeight = 1.0;
        previousSum += 1.0;
    }

    for (uint32_t iteration = 0; iteration < config_.maxWeightIterations; ++iteration) {
        // Refit is required each iteration because fitQuadraticLogisticRegression()
        // and fitLogisticMean() both consume sample.designWeight that is changed in the inner loop.
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

std::vector<double> CLOPOptimizer::computeEstimatedOptimum(const std::vector<CLOPSample>& samples) const {
    if (samples.empty()) {
        std::vector<double> defaults;
        defaults.reserve(config_.parameters.size());
        for (const auto& parameter : config_.parameters) {
            defaults.push_back(parameter.defaultValue);
        }
        return defaults;
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
        std::vector<double> defaults;
        defaults.reserve(config_.parameters.size());
        for (const auto& parameter : config_.parameters) {
            defaults.push_back(parameter.defaultValue);
        }
        return defaults;
    }

    std::vector<double> estimated(weightedSum.size(), 0.0);
    for (size_t parameterIndex = 0; parameterIndex < weightedSum.size(); ++parameterIndex) {
        estimated[parameterIndex] = weightedSum[parameterIndex] / totalWeight;
    }

    return estimated;
}

size_t CLOPOptimizer::featureCount() const {
    const size_t dimension = config_.parameters.size();
    const size_t linearTerms = dimension;
    const size_t diagonalTerms = dimension;
    const size_t crossTerms = dimension * (dimension - 1) / 2;
    return 1 + linearTerms + diagonalTerms + crossTerms;
}

std::vector<double> CLOPOptimizer::buildFeatureVector(const std::vector<double>& normalizedValues) const {
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

double CLOPOptimizer::evaluateQuadratic(
    const LogisticModel& model,
    const std::vector<double>& normalizedValues) const {

    const auto featureVector = buildFeatureVector(normalizedValues);
    return std::inner_product(
        model.coefficients.begin(),
        model.coefficients.end(),
        featureVector.begin(),
        0.0);
}

TableData CLOPOptimizer::getStatusTable() const {
    std::scoped_lock lock(stateMutex_);

    TableData table;
    table.columnWidths = { 24, 12, 12, 12, 14 };
    table.headers = { "Parameter", "Initial", "Estimated", "Min", "Max" };

    for (size_t index = 0; index < config_.parameters.size(); ++index) {
        const auto& parameter = config_.parameters[index];
        table.body.push_back({
            parameter.name,
            parameter.defaultValue,
            estimatedParameters_[index],
            parameter.minValue,
            parameter.maxValue
        });
    }

    return table;
}

} // namespace QaplaTester
