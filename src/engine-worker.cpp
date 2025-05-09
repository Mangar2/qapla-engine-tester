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

#include <stdexcept>

EngineWorker::EngineWorker(std::unique_ptr<EngineAdapter> adapter)
    : adapter_(std::move(adapter)),
    thread_(&EngineWorker::threadLoop, this) {
    if (!adapter_) {
        throw std::invalid_argument("EngineWorker requires a valid EngineAdapter");
    }

    post([](EngineAdapter& adapter) {
        adapter.runEngine();  
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

    post(std::nullopt);  // Shutdown-Signal
    cv_.notify_all();

    if (thread_.joinable()) {
        thread_.join();
    }
}

/**
 * @brief Main execution loop for the worker thread.
 */
void EngineWorker::threadLoop() {
    while (true) {
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