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

EngineWorker::EngineWorker(std::unique_ptr<EngineAdapter> adapter, std::string identifier)
    : adapter_(std::move(adapter)), identifier_(identifier)
    {

    if (!adapter_) {
        throw std::invalid_argument("EngineWorker requires a valid EngineAdapter");
    }

    adapter_->setLogger([id = identifier_](std::string_view message, bool isOutput) {
        Logger::instance().log(id, message, isOutput);
        });
    running_ = true;
    workThread_ = std::thread(&EngineWorker::threadLoop, this);
    startupFuture_ = startupPromise_.get_future();

    post([this](EngineAdapter& adapter) {
        try {
            adapter.runEngine();
            readThread_ = std::thread(&EngineWorker::readLoop, this);

            adapter.askForReady();
            if (!waitForReady(ReadyTimeoutStartup)) {
                throw std::runtime_error("Engine failed startup readiness check");
            }

            startupPromise_.set_value(); // Erfolg
        }
        catch (...) {
            startupPromise_.set_exception(std::current_exception()); // Fehler
        }
        });
}

EngineWorker::~EngineWorker() {
    stop();
}

void EngineWorker::stop() {
    post([](EngineAdapter& adapter) {
        try {
            adapter.terminateEngine(); 
        }
        catch (...) {
            // Nothing to do, if we cannot stop it, we can do nothing else
        }
        });
    running_ = false;
    post(std::nullopt);  // Shutdown-Signal
    cv_.notify_all();

    if (workThread_.joinable()) {
        workThread_.join();
    }
    
	if (readThread_.joinable()) {
		readThread_.join();
	}
}

/**
 * @brief Main execution loop for the worker thread.
 */
void EngineWorker::threadLoop() {
    while (running_) {
        std::optional<std::function<void(EngineAdapter&)>> task;

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return !taskQueue_.empty(); });

            task = std::move(taskQueue_.front());
            taskQueue_.pop();
        }

        if (!task.has_value()) {
            break; // Shutdown-Signal
        }

        try {
            (*task)(*adapter_);
        }
        catch (...) {
            // optional: Logging
        }
    }
}

void EngineWorker::post(std::optional<std::function<void(EngineAdapter&)>> task) {
    {
        std::scoped_lock lock(mutex_);
        taskQueue_.push(std::move(task));
    }
    cv_.notify_one();
}

bool EngineWorker::waitForReady(std::chrono::milliseconds timeout) {
    std::unique_lock lock(readyMutex_);
    readyReceived_ = false;
    return readyCv_.wait_for(lock, timeout, [this] {
        return readyReceived_;
    });
}

bool EngineWorker::startupReady() {
    post([](EngineAdapter& adapter) {
        adapter.askForReady();
        });
    return waitForReady(ReadyTimeoutStartup);
}

void EngineWorker::computeMove(const GameState& gameState, const GoLimits& limits) {
    post([this, gameState, limits](EngineAdapter& adapter) {
        try {
            int64_t sendTimestamp = adapter.computeMove(gameState, limits);
            if (eventSink_) {
				eventSink_(EngineEvent{ EngineEvent::Type::ComputeMoveSent, sendTimestamp });
            }
        }
        catch (...) {
            // TODO: Fehlerereignis an GameManager senden
        }
        });
}

void EngineWorker::readLoop() {
    while (running_) {
        EngineEvent event = adapter_->readEvent(); // blockierender Aufruf

        // Synchronisationslogik für readyok
        if (event.type == EngineEvent::Type::ReadyOk) {
            {
                std::scoped_lock lock(readyMutex_);
                readyReceived_ = true;
            }
            readyCv_.notify_all();
        }

        // Weiterleitung an übergeordnete Spielsteuerung
        if (eventSink_) {
            eventSink_(event);
        }
    }
}