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

#include "engine-worker.h"
#include "engine-adapter.h"  
#include "logger.h"
#include "timer.h"

#include <stdexcept>

namespace QaplaTester {

using QaplaHelpers::Timer;

EngineWorker::EngineWorker(std::unique_ptr<EngineAdapter> adapter, std::string identifier, 
    const EngineConfig& engineConfig)
    : identifier_(std::move(identifier)), adapter_(std::move(adapter))
{
    cliTraceLevel_ = Logger::engineLogger().getCliThreshold();
    if (!adapter_) {
        throw std::invalid_argument("Internal Error: EngineWorker requires a valid EngineAdapter");
    }
    engineConfig_ = engineConfig;

    adapter_->setProtocolLogger([this, id = identifier_](std::string_view message, bool isOutput, TraceLevel traceLevel) {
        Logger::engineLogger().log(id, message, isOutput, cliTraceLevel_, engineConfig_.getTraceLevel(), traceLevel);
        
        });
    
	asyncStartup(engineConfig.getOptionValues());
}

void EngineWorker::asyncStartup(const OptionValues& optionValues) {
	workerState_ = WorkerState::starting;
    writeThread_ = std::thread(&EngineWorker::writeLoop, this);
    startupFuture_ = startupPromise_.get_future();

    post([this, options = optionValues](EngineAdapter& adapter) {
        try {
            readThread_ = std::thread(&EngineWorker::readLoop, this);
            // Define expected response for the reader before initiating the protocol command.
            // This ensures the read thread knows which handshake response to watch for.
            waitForHandshake_ = EngineEvent::Type::ProtocolOk;
            adapter.startProtocol();
            if (!waitForHandshake(ReadyTimeoutProtocolOk)) {
                if (adapter.isProtocolOkRequired()) {
                    throw std::runtime_error("Engine " + getEngineName() + " failed UCI handshake");
                }
            }
            if (!options.empty()) {
                adapter.setOptionValues(options);
				waitForHandshake_ = EngineEvent::Type::ReadyOk;
                adapter.askForReady();
                if (!waitForHandshake(ReadyTimeoutOption)) {
                    throw std::runtime_error("Engine " + getEngineName() + " failed ready ok handshake after setoptions");
                }
            }
            adapter.setPonder(engineConfig_.isPonderEnabled());
            startupPromise_.set_value(); 
			workerState_ = WorkerState::running;
        }
        catch (...) {
            workerState_ = WorkerState::failure;
            startupPromise_.set_exception(std::current_exception()); 
        }
        });
}

EngineWorker::~EngineWorker() {
    stop(true);
}

void EngineWorker::stop(bool wait) {
    if (workerState_ != WorkerState::stopped) {
        workerState_ = WorkerState::stopped;
        post([](EngineAdapter& adapter) {
            try {
                adapter.terminateEngine();
            }
            catch (...) {
                // Nothing to do, if we cannot stop it, we can do nothing else
            }
            });
        post(std::nullopt);  // Shutdown-Signal
        cv_.notify_all();
    }

    if (wait) {
        if (writeThread_.joinable()) {
            writeThread_.join();
        }

        if (readThread_.joinable()) {
            readThread_.join();
        }
    }
}

/**
 * @brief Main execution loop for the worker thread.
 */
void EngineWorker::writeLoop() {
    if (workerState_ == WorkerState::stopped) {
        return;
    }
    while (true) {
        std::optional<std::function<void(EngineAdapter&)>> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { return !writeQueue_.empty(); });

            task = std::move(writeQueue_.front());
            writeQueue_.pop();
        }

        if (!task.has_value()) {
            break; // Shutdown-Signal
        }

        try {
            (*task)(*adapter_);
        }
        
        catch (const std::exception& e) {
            // Usually the engine disconnected this is reported as error elswhere
            // Thus we log it with TraceLevel:info only
            Logger::testLogger().log("Exception in threadLoop, id " + getIdentifier() + " " 
                + std::string(e.what()), TraceLevel::info);
        }
        catch (...) {
            Logger::testLogger().log("Unknown exception in threadLoop, id " + getIdentifier(), 
                TraceLevel::error);
        }
    }
}

void EngineWorker::post(std::optional<std::function<void(EngineAdapter&)>> task) {
    {
        std::scoped_lock lock(mutex_);
        writeQueue_.push(std::move(task));
    }
    cv_.notify_one();
}

bool EngineWorker::waitForHandshake(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(handshakeMutex_);
    if (!handshakeReceived_) {
        handshakeCv_.wait_for(lock, timeout, [this] {
            return handshakeReceived_;
            });
    }
    bool received = handshakeReceived_;
    handshakeReceived_ = false;
    return received;
}

bool EngineWorker::requestReady(std::chrono::milliseconds timeout) {
    post([this](EngineAdapter& adapter) {
        waitForHandshake_ = EngineEvent::Type::ReadyOk;
        adapter.askForReady();
        });
    return waitForHandshake(timeout);
}

bool EngineWorker::moveNow(bool wait, std::chrono::milliseconds timeout) {
    post([this, wait](EngineAdapter& adapter) {
        waitForHandshake_ = EngineEvent::Type::None;
        if (wait) {
            waitForHandshake_ = adapter.waitAfterMoveNowHandshake();
            if (waitForHandshake_ == EngineEvent::Type::None) {
                // Notify for handshake right away
                {
                    std::scoped_lock lock(handshakeMutex_);
                    handshakeReceived_ = true;
                }
                handshakeCv_.notify_all();
            }
        }
        adapter.moveNow();
        });
    if (!wait) {
        return true;
    }
    return waitForHandshake(timeout);
}

bool EngineWorker::handlePonderMiss(std::chrono::milliseconds timeout) {
    post([this](EngineAdapter& adapter) {
        waitForHandshake_ = adapter.handlePonderMiss();
        if (waitForHandshake_ == EngineEvent::Type::None) {
            // Notify for handshake right away (XBoard case)
            {
                std::scoped_lock lock(handshakeMutex_);
                handshakeReceived_ = true;
            }
            handshakeCv_.notify_all();
        }
        });
    return waitForHandshake(timeout);
}

bool EngineWorker::setOption(const std::string& name, const std::string& value) {
    post([this, name, value](EngineAdapter& adapter) {
        waitForHandshake_ = EngineEvent::Type::ReadyOk;
        adapter.setTestOption(name, value);
        adapter.askForReady();
        });
    return waitForHandshake(ReadyTimeoutOption);
}

bool EngineWorker::setOptionValues(const OptionValues& optionValues) {
	post([this, optionValues](EngineAdapter& adapter) {
        waitForHandshake_ = EngineEvent::Type::ReadyOk;
		adapter.setOptionValues(optionValues);
		adapter.askForReady();
		});
	return waitForHandshake(ReadyTimeoutOption);
}

void EngineWorker::computeMove(const GameRecord& gameRecord, const GoLimits& limits, bool ponderHit) {
    GameStruct game = gameRecord.createGameStruct();
    post([this, game = std::move(game), limits, ponderHit](EngineAdapter& adapter) {
        try {
            if (eventSink_) {
                // This ensures that all remaining info packets from pondering arrive before this marker,
                // allowing the GameManager to safely distinguish between stale and current compute data.
                eventSink_(EngineEvent::create(EngineEvent::Type::SendingComputeMove, identifier_, 
                    Timer::getCurrentTimeMs()));
            }
            uint64_t sendTimestamp = adapter.computeMove(game, limits, ponderHit);
            if (eventSink_) {
				eventSink_(EngineEvent::create(EngineEvent::Type::ComputeMoveSent, identifier_, sendTimestamp));
            }
        }
        catch (const std::exception& ex) {
            if (eventSink_) {
                auto e = EngineEvent::create(EngineEvent::Type::ComputeMoveSent, identifier_,
                    Timer::getCurrentTimeMs());
                e.errors.push_back({ 
                    .name = "I/O Error", 
                    .detail = std::string("Failed to send compute move command: ") + ex.what(),
					.level = TraceLevel::error
                    });
                eventSink_(std::move(e));
            }
        }
        catch (...) {
            if (eventSink_) {
                auto e = EngineEvent::create(EngineEvent::Type::ComputeMoveSent, identifier_,
                    Timer::getCurrentTimeMs());
                e.errors.push_back({ 
                    .name = "I/O Error",
                    .detail = std::string("Failed to send compute move command"),
                    .level = TraceLevel::error
                });
                eventSink_(std::move(e));
            }
        }
        });
}

void EngineWorker::allowPonder(const GameRecord& gameRecord, const GoLimits& limits, 
    const std::string& ponderMove) {
    GameStruct game = gameRecord.createGameStruct();
    post([this, game, limits, ponderMove](EngineAdapter& adapter) {
        try {
			uint64_t sendTimestamp = adapter.allowPonder(game, limits, ponderMove);
            if (eventSink_) {
                eventSink_(EngineEvent::create(EngineEvent::Type::PonderMoveSent, identifier_, sendTimestamp));
            }
        }
        catch (...) {
            if (eventSink_) {
                auto error = EngineEvent::create(EngineEvent::Type::PonderMoveSent, identifier_, 
                    Timer::getCurrentTimeMs());
                error.errors.push_back({ .name = "I/O Error", .detail = "Failed to send go ponder command" });
                eventSink_(std::move(error));
            }
        }
        });
}

void EngineWorker::readLoop() {
	// Must end on disconnected_ to prevent endless looping
    while (workerState_ != WorkerState::stopped && !disconnected_) {
        // Blocking call
        try {
            EngineEvent event = adapter_->readEvent();

            if (event.type == waitForHandshake_) {
                {
                    std::scoped_lock lock(handshakeMutex_);
                    // We wait for a single handshake. waitForHandshake_ must be set
                    // again for each new handshake request.
                    waitForHandshake_ = EngineEvent::Type::None;
                    handshakeReceived_ = true;
                }
                handshakeCv_.notify_all();
                continue;
            }

			if (event.type == EngineEvent::Type::None || event.type == EngineEvent::Type::NoData) {
				continue; 
			}

            if (eventSink_) {
                eventSink_(std::move(event));
            }
			if (event.type == EngineEvent::Type::EngineDisconnected) {
				// disconnected engines would lead to endless looping so we need to terminate the read thread
				disconnected_ = true;
                workerState_ = WorkerState::failure;
			}
        }
		catch (const std::exception& e) {
			Logger::testLogger().log("Exception in readLoop, id " + getIdentifier() + " "
				+ std::string(e.what()), TraceLevel::error);
		}
		catch (...) {
			Logger::testLogger().log("Unknown exception in readLoop, id " + getIdentifier(),
				TraceLevel::error);
		}
    }
}

} // namespace QaplaTester
