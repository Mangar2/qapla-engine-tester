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
#include "windows.h"

#include <stdexcept>

EngineWorker::EngineWorker(std::unique_ptr<EngineAdapter> adapter, std::string identifier, const OptionValues& optionValues)
    : adapter_(std::move(adapter)), identifier_(identifier)
    {

    if (!adapter_) {
        throw std::invalid_argument("EngineWorker requires a valid EngineAdapter");
    }

    adapter_->setProtocolLogger([id = identifier_](std::string_view message, bool isOutput, TraceLevel traceLevel) {
        Logger::engineLogger().log(id, message, isOutput, traceLevel);
        });
	asyncStartup(optionValues);
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
            waitForHandshake_ = EngineEvent::Type::UciOk;
            adapter.startProtocol();
            if (!waitForHandshake(ReadyTimeoutUciOk)) {
                throw std::runtime_error("Engine failed UCI handshake");
            }
            if (!options.empty()) {
                adapter.setOptionValues(options);
				waitForHandshake_ = EngineEvent::Type::ReadyOk;
                adapter.askForReady();
                if (!waitForHandshake(ReadyTimeoutOption)) {
                    throw std::runtime_error("Engine failed ready ok handshake after setoptions");
                }
            }
            startupPromise_.set_value(); 
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

void EngineWorker::restart() {
	adapter_->restartEngine();
}

/**
 * @brief Main execution loop for the worker thread.
 */
void EngineWorker::writeLoop() {
    if (workerState_ == WorkerState::stopped) return;
    while (true) {
        std::optional<std::function<void(EngineAdapter&)>> task;

        {
            std::unique_lock lock(mutex_);
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
    std::unique_lock lock(handshakeMutex_);
    handshakeReceived_ = false;
    return handshakeCv_.wait_for(lock, timeout, [this] {
        return handshakeReceived_;
    });
}

bool EngineWorker::requestReady(std::chrono::milliseconds timeout) {
    post([this](EngineAdapter& adapter) {
        waitForHandshake_ = EngineEvent::Type::ReadyOk;
        adapter.askForReady();
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

void EngineWorker::computeMove(const GameRecord& gameRecord, const GoLimits& limits) {
    post([this, gameRecord, limits](EngineAdapter& adapter) {
        try {
            int64_t sendTimestamp = adapter.computeMove(gameRecord, limits);
            if (eventSink_) {
				eventSink_(EngineEvent::create(EngineEvent::Type::ComputeMoveSent, identifier_, sendTimestamp));
            }
        }
        catch (...) {
            // TODO: Fehlerereignis an GameManager senden
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
                    handshakeReceived_ = true;
                }
                handshakeCv_.notify_all();
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