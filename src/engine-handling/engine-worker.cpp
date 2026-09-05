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

#include <stdexcept>
#include <format>

#include "engine-worker.h"
#include "engine-adapter.h"  
#include "../base-elements/logger.h"
#include "../base-elements/timer.h"

namespace QaplaTester {

using QaplaHelpers::Timer;

EngineWorker::EngineWorker(std::unique_ptr<EngineAdapter> adapter, std::string identifier, 
    const EngineConfig& engineConfig)
    : identifier_(std::move(identifier)), adapter_(std::move(adapter))
{
    cliTraceLevel_ = EngineLogger::engineLogger().getCliThreshold();
    if (!adapter_) {
        throw std::invalid_argument("Internal Error: EngineWorker requires a valid EngineAdapter");
    }
    engineConfig_ = engineConfig;

    adapter_->setProtocolLogger([this, id = identifier_](std::string_view message, bool isOutput, TraceLevel traceLevel) {
        EngineLogger::engineLogger({.engineId = id}).log(
            id, message, isOutput, cliTraceLevel_, engineConfig_.getTraceLevel(), traceLevel);
        });
    
	asyncStartup(engineConfig.getOptionValues());
}

void EngineWorker::logNote(std::string_view message, TraceLevel level) const {
    EngineLogger::engineLogger({.engineId = identifier_}).logNote(
        message, cliTraceLevel_, engineConfig_.getTraceLevel(), level);
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
            armHandshake(EngineEvent::Type::ProtocolOk);
            adapter.startProtocol();
            if (!waitForHandshake(ReadyTimeoutProtocolOk)) {
                if (adapter.isProtocolOkRequired()) {
                    throw std::runtime_error("Engine " + getEngineName() + " failed UCI handshake");
                }
            }
            if (!options.empty()) {
                adapter.setOptionValues(options);
                armHandshake(EngineEvent::Type::ReadyOk);
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
            catch (...) { // NOLINT(bugprone-empty-catch)
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
    if (workerState_ == WorkerState::stopped || workerState_ == WorkerState::failure) {
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
            Logger::reportLogger().log("Exception in threadLoop, id " + getIdentifier() + " " 
                + std::string(e.what()), TraceLevel::info);
        }
        catch (...) {
            Logger::reportLogger().log("Unknown exception in threadLoop, id " + getIdentifier(), 
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
    if (!received) {
        // Timed out: disarm the handshake. A late reply is then forwarded to the GameManager
        // as a regular event (fenced by the SendingComputeMove marker) instead of staying
        // armed and swallowing the bestmove of a later compute command.
        waitForHandshake_ = EngineEvent::Type::None;
    }
    return received;
}

bool EngineWorker::requestReady(std::chrono::milliseconds timeout) {
    post([this](EngineAdapter& adapter) {
        armHandshake(EngineEvent::Type::ReadyOk);
        adapter.askForReady();
        });
    return waitForHandshake(timeout);
}

bool EngineWorker::moveNow(bool wait, std::chrono::milliseconds timeout) {
    post([this, wait](EngineAdapter& adapter) {
        auto handshakeType = wait ? adapter.waitAfterMoveNowHandshake() : EngineEvent::Type::None;
        armHandshake(handshakeType);
        adapter.moveNow();
        if (wait && handshakeType == EngineEvent::Type::None) {
            // No handshake possible, notify right away
            notifyHandshake();
        }
        });
    if (!wait) {
        return true;
    }
    return waitForHandshake(timeout);
}

bool EngineWorker::stopCompute(bool wait, std::chrono::milliseconds timeout) {
    post([this, wait](EngineAdapter& adapter) {
        auto handshakeType = wait ? adapter.waitAfterMoveNowHandshake() : EngineEvent::Type::None;
        armHandshake(handshakeType);
        adapter.stop();
        if (wait && handshakeType == EngineEvent::Type::None) {
            // No handshake possible, notify right away
            notifyHandshake();
        }
        });
    if (!wait) {
        return true;
    }
    return waitForHandshake(timeout);
}

bool EngineWorker::handlePonderMiss(std::chrono::milliseconds timeout) {
    post([this](EngineAdapter& adapter) {
        // Arm the handshake before handlePonderMiss() sends anything. Otherwise the engine's
        // bestmove can cross with the arming: it is then forwarded as a regular event while
        // the handshake stays armed and swallows the bestmove of the next compute command.
        auto handshakeType = adapter.waitAfterPonderMissHandshake();
        armHandshake(handshakeType);
        adapter.handlePonderMiss();
        if (handshakeType == EngineEvent::Type::None) {
            // Notify for handshake right away (XBoard case)
            notifyHandshake();
        }
        });
    return waitForHandshake(timeout);
}

bool EngineWorker::setOption(const std::string& name, const std::string& value) {
    post([this, name, value](EngineAdapter& adapter) {
        armHandshake(EngineEvent::Type::ReadyOk);
        adapter.setTestOption(name, value);
        adapter.askForReady();
        });
    return waitForHandshake(ReadyTimeoutOption);
}

bool EngineWorker::setOptionValues(const OptionValues& optionValues) {
	post([this, optionValues](EngineAdapter& adapter) {
        armHandshake(EngineEvent::Type::ReadyOk);
		adapter.setOptionValues(optionValues);
		adapter.askForReady();
		});
	return waitForHandshake(ReadyTimeoutOption);
}

void EngineWorker::computeMove(const GameRecord& gameRecord, const GoLimits& limits, bool ponderHit) {
    GameStruct game = gameRecord.createGameStruct();
    post([this, game = std::move(game), limits, ponderHit](EngineAdapter& adapter) {
        try {
            // This ensures that all remaining info packets from pondering arrive before this marker,
            // allowing the GameManager to safely distinguish between stale and current compute data.
            sendEvent(EngineEvent::create(EngineEvent::Type::SendingComputeMove, identifier_, 
                Timer::getCurrentTimeMs()));

            uint64_t sendTimestamp = adapter.computeMove(game, limits, ponderHit);
            sendEvent(EngineEvent::create(EngineEvent::Type::ComputeMoveSent, identifier_, sendTimestamp));
        }
        catch (const std::exception& ex) {
            auto e = EngineEvent::create(EngineEvent::Type::ComputeMoveSent, identifier_,
                Timer::getCurrentTimeMs());
            e.errors.push_back({ 
                .name = "I/O Error", 
                .detail = std::string("Failed to send compute move command: ") + ex.what(),
                .level = TraceLevel::error
                });
            sendEvent(std::move(e));
        }
        catch (...) {
            auto e = EngineEvent::create(EngineEvent::Type::ComputeMoveSent, identifier_,
                Timer::getCurrentTimeMs());
            e.errors.push_back({ 
                .name = "I/O Error",
                .detail = std::string("Failed to send compute move command"),
                .level = TraceLevel::error
            });
            sendEvent(std::move(e));
        }
        });
}

void EngineWorker::allowPonder(const GameRecord& gameRecord, const GoLimits& limits, 
    const std::string& ponderMove) {
    GameStruct game = gameRecord.createGameStruct();
    post([this, game, limits, ponderMove](EngineAdapter& adapter) {
        try {
			uint64_t sendTimestamp = adapter.allowPonder(game, limits, ponderMove);
            sendEvent(EngineEvent::create(EngineEvent::Type::PonderMoveSent, identifier_, sendTimestamp));
        }
        catch (...) {
            auto error = EngineEvent::create(EngineEvent::Type::PonderMoveSent, identifier_, 
                Timer::getCurrentTimeMs());
            error.errors.push_back({ .name = "I/O Error", .detail = "Failed to send go ponder command" });
            sendEvent(std::move(error));
        }
        });
}

void EngineWorker::readLoop() {
	// Must end on disconnected_ to prevent endless looping
    while (workerState_ != WorkerState::stopped && workerState_ != WorkerState::failure && !disconnected_) {
        // Blocking call
        try {
            EngineEvent event = adapter_->readEvent();

            bool isHandshake = false;
            {
                std::scoped_lock lock(handshakeMutex_);
                // Type::None must never match: it is the disarmed state, and a None event
                // matching it would set a stale handshakeReceived_ flag causing a later
                // waitForHandshake() to return immediately without a real handshake.
                if (waitForHandshake_ != EngineEvent::Type::None && event.type == waitForHandshake_) {
                    // We wait for a single handshake. waitForHandshake_ must be set
                    // again for each new handshake request.
                    waitForHandshake_ = EngineEvent::Type::None;
                    handshakeReceived_ = true;
                    isHandshake = true;
                }
            }
            if (isHandshake) {
                handshakeCv_.notify_all();
                continue;
            }

			if (event.type == EngineEvent::Type::None || event.type == EngineEvent::Type::NoData) {
				continue; 
			}

			if (event.type == EngineEvent::Type::EngineDisconnected) {
				// disconnected engines would lead to endless looping so we need to terminate the read thread
				disconnected_ = true;
                if (workerState_ == WorkerState::stopped) {
                    // The engine was stopped intentionally (quit has been sent, e.g. for a restart
                    // with new options). The closed pipe is expected here - neither an error nor
                    // a disconnect event for the GameManager.
                    logNote(std::format("Engine {}, id {} terminated after quit (expected)",
                        getEngineName(), getIdentifier()), TraceLevel::info);
                    continue;
                }
                workerState_ = WorkerState::failure;
                std::string msg = std::format("Engine {}, id {} disconnected", getEngineName(), getIdentifier());
                Logger::reportLogger().log(msg, TraceLevel::error);
                logNote(msg, TraceLevel::error);
			}
            sendEvent(std::move(event));
        }
		catch (const std::exception& e) {
			Logger::reportLogger().log("Exception in readLoop, id " + getIdentifier() + " "
				+ std::string(e.what()), TraceLevel::error);
		}
		catch (...) {
			Logger::reportLogger().log("Unknown exception in readLoop, id " + getIdentifier(),
				TraceLevel::error);
		}
    }
}

void EngineWorker::sendEvent(EngineEvent&& event) const {
	std::scoped_lock lock(eventSinkMutex_);
	if (eventSink_) {
		eventSink_(std::move(event));
	}
}

} // namespace QaplaTester
