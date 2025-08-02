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

#include <vector>
#include <mutex>
#include <functional>
#include <optional>
#include <algorithm>

#include "engine-event.h"


 /**
  * @brief A thread-safe sink that records all EngineEvents for analysis in tests.
  */
class EventSinkRecorder {
public:
    /**
     * @brief Returns a callback suitable for EngineWorker::setEventSink.
     */
    std::function<void(EngineEvent&&)> getCallback() {
        return [this](EngineEvent&& event) {
            std::lock_guard<std::mutex> lock(mutex_);
            events_.emplace_back(std::move(event));
            };
    }

    /**
     * @brief Returns the number of recorded events of the given type.
     */
    std::size_t count(EngineEvent::Type type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<std::size_t>
            (std::count_if(events_.begin(), events_.end(), [type](const EngineEvent& e) {
                return e.type == type;
            }));
    }

    /**
     * @brief Returns all recorded events.
     */
    std::vector<EngineEvent> getAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_;
    }

    /**
     * @brief Returns the last recorded event of the given type, if any.
     */
    std::optional<EngineEvent> getLastOfType(EngineEvent::Type type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
            if (it->type == type) {
                return *it;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Clears all recorded events.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<EngineEvent> events_;
};
