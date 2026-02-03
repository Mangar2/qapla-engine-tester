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
 * @author Volker BÃ¶hm
 * @copyright Copyright (c) 2025 Volker BÃ¶hm
 */

#include "engine-worker-factory.h"
#include "uci-adapter.h"
#include "winboard-adapter.h"
#include "engine-config-manager.h"

#include "../engine-tester/engine-report.h"

constexpr int DEFAULT_MAX_PARALLEL_STARTS = 2;    ///> Default max parallel engine starts
constexpr int ENGINE_START_RETRY = 3;             ///> Number of retries for engine start
constexpr int ENGINE_START_RETRY_DELAY_MS = 2000; ///> Delay between retries in milliseconds

namespace QaplaTester {

// RAII helper class to manage engine start slots
class EngineStartGuard {
public:
    EngineStartGuard() {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait until a slot is available
        condition_.wait(lock, [] {
            return currentActiveStarts_ < maxParallelStarts_;
        });
        // Acquired a slot
        currentActiveStarts_++;
    }

    ~EngineStartGuard() {
        std::scoped_lock lock(mutex_);
        currentActiveStarts_--;

        // Notify one waiting thread that a slot is now available
        condition_.notify_one();
    }

    // Delete copy and move to ensure RAII semantics
    EngineStartGuard(const EngineStartGuard&) = delete;
    EngineStartGuard& operator=(const EngineStartGuard&) = delete;
    EngineStartGuard(EngineStartGuard&&) = delete;
    EngineStartGuard& operator=(EngineStartGuard&&) = delete;

    // Static configuration methods
    static void setMaxParallelStarts(int maxParallel) {
        std::scoped_lock lock(mutex_);
        maxParallelStarts_ = maxParallel;
        // Wake up all waiting threads in case limit was increased
        condition_.notify_all();
    }

    static int getMaxParallelStarts() {
        std::scoped_lock lock(mutex_);
        return maxParallelStarts_;
    }

    static int getCurrentActiveStarts() {
        std::scoped_lock lock(mutex_);
        return currentActiveStarts_;
    }

private:
    static inline int maxParallelStarts_ = DEFAULT_MAX_PARALLEL_STARTS;
    static inline int currentActiveStarts_ = 0;
    static inline std::mutex mutex_;
    static inline std::condition_variable condition_;
};

void EngineWorkerFactory::assignUniqueDisplayNames() {
    auto& engines = getActiveEnginesMutable();
    EngineConfigManager::assignUniqueDisplayNames(engines);
}

void EngineWorkerFactory::setMaxParallelStarts(int maxParallel) {
    EngineStartGuard::setMaxParallelStarts(maxParallel);
}

int EngineWorkerFactory::getMaxParallelStarts() {
    return EngineStartGuard::getMaxParallelStarts();
}

int EngineWorkerFactory::getCurrentActiveStarts() {
    return EngineStartGuard::getCurrentActiveStarts();
}

std::unique_ptr<EngineWorker> EngineWorkerFactory::createEngine(const EngineConfig& config) {
    // Acquire a start slot - this will block if too many engines are starting
    EngineStartGuard guard;
    
    // Atomic fetch_add ensures each engine gets a unique identifier
    auto id = identifier_.fetch_add(1, std::memory_order_relaxed);
    
    EngineStartupParams params {
        .executablePath = config.getCmd(),
        .workingDirectory = config.getDir(),
        .identifierStr = "#" + std::to_string(id),
        .executableArguments = config.getArgs()
    };
    
    std::unique_ptr<EngineAdapter> adapter;
    if (config.getProtocol() == EngineProtocol::Uci) {
		adapter = std::make_unique<UciAdapter>(params);
	}
    else if (config.getProtocol() == EngineProtocol::XBoard) {
        adapter = std::make_unique<WinboardAdapter>(params);
    } 
    else {
        throw AppError::makeInvalidParameters("Unsupported engine protocol: " + to_string(config.getProtocol()));
    }
    adapter->setSuppressInfoLines(rapid_);
    auto worker = std::make_unique<EngineWorker>(std::move(adapter), params.identifierStr, config);
    return worker;
    // guard destructor automatically releases the slot
}

std::unique_ptr<EngineWorker> EngineWorkerFactory::restart(const EngineWorker& worker) {
	return createEngine(worker.getConfig());
}

void EngineWorkerFactory::waitUntilAllEnginesStarted(std::vector<std::future<void>>& futures, 
    const EngineConfig& config) {
    auto checklist = EngineReport::getChecklist(config.getName());
    for (auto& f : futures) {
        try {
            f.get();
        }
        catch (const std::exception& error) {
            checklist->logReport("starts-and-stops-cleanly", false, std::string(error.what()));
        }
        catch (...) {
            checklist->logReport("starts-and-stops-cleanly", false, "Unknown error");
        }
    }
}

void EngineWorkerFactory::waitUntilAllEnginesStarted(std::vector<std::future<void>>& futures, 
    const std::vector<EngineConfig>& configs) {
    uint32_t index = 0;
    for (auto& f : futures) {
        const auto& name = configs[index].getName();
        try {
            f.get();
        }
        catch (const std::exception& error) {
            if (!name.empty()) {
                auto checklist = EngineReport::getChecklist(name);
                checklist->logReport("starts-and-stops-cleanly", false, std::string(error.what()));
            }
        }
        catch (...) {
            if (!name.empty()) {
                auto checklist = EngineReport::getChecklist(name);
                checklist->logReport("starts-and-stops-cleanly", false, "Unknown error");
            }
        }
        index++;
    }
}

EngineList EngineWorkerFactory::createEngines(const EngineConfig& config, std::size_t count) {
    EngineList engines;
    std::vector<std::future<void>> futures;
    engines.reserve(count);
    for (int retry = 0; retry < ENGINE_START_RETRY; retry++) {
        futures.clear();
        bool waitOnFailure = true;
        for (std::size_t i = 0; i < count; ++i) {
            // We initialize all engines in the first loop
            if (engines.size() <= i) {
                engines.push_back(createEngine(config));
                futures.push_back(engines.back()->getStartupFuture());
            }
            else if (engines[i]->failure()) {
                if (waitOnFailure) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(ENGINE_START_RETRY_DELAY_MS));
                    waitOnFailure = false;
                }
                // The retry loops recreate engines having exceptions in the startup process
                engines[i] = createEngine(config);
                futures.push_back(engines[i]->getStartupFuture());
            }
        }
        // Wait for all newly created engines.
        waitUntilAllEnginesStarted(futures, config);
    }

    EngineList runningEngines;
    for (auto& engine : engines) {
        if (!engine->failure()) {
            runningEngines.push_back(std::move(engine));
        }
    }
    return runningEngines;
}

EngineList EngineWorkerFactory::createEngines(const std::vector<EngineConfig>& configs, bool noWait) {
    EngineList engines;
    std::vector<std::future<void>> futures;
    engines.reserve(configs.size());
    for (int retry = 0; retry < ENGINE_START_RETRY; retry++) {
        futures.clear();
        uint32_t index = 0;
        bool waitOnFailure = true;
        for (const auto& config: configs) {
            // We initialize all engines in the first loop
            if (engines.size() <= index) {
                try {
                    engines.push_back(createEngine(config));
                    futures.push_back(engines.back()->getStartupFuture());
                } 
                catch (const std::exception& error) {
                    EngineReport::getChecklist(config.getName())
                        ->logReport("starts-and-stops-cleanly", false, std::string(error.what()));
                }
            }
            else if (engines[index]->failure()) {
                if (waitOnFailure) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(ENGINE_START_RETRY_DELAY_MS));
                    waitOnFailure = false;
                }
                // The retry loops recreate engines having exceptions in the startup process
                engines[index] = createEngine(config);
                futures.push_back(engines[index]->getStartupFuture());
            }
            index++;
        }
        if (noWait) {
            // If noWait is true, we skip waiting for the startup futures.
            // This allows engines to start asynchronously.
            continue;
        }
        // Wait for all newly created engines.
        waitUntilAllEnginesStarted(futures, configs);
    }    
    EngineList runningEngines;
    for (auto& engine : engines) {
        if (noWait || !engine->failure()) {
            runningEngines.push_back(std::move(engine));
        }
    }
    return runningEngines;
}

} // namespace QaplaTester
