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
 * @copyright Copyright (c) 2026 Volker Böhm
 */
#pragma once

#include "timer.h"
#include <functional>

namespace QaplaHelpers {

class CallbackTimer {
public:
    CallbackTimer() = default;

    CallbackTimer(uint32_t intervalMs, std::function<void()> callback)
        : intervalMs_(intervalMs), callback_(std::move(callback)) {
        timer_.start();
    }

    void update(bool force = false) {
        if (!callback_) {
            return;
        }

        if (force || firstCall_ || (intervalMs_ > 0 && timer_.elapsedMs() >= intervalMs_)) {
            callback_();
            timer_.start();
            firstCall_ = false;
        }
    }

    void setInterval(uint32_t intervalMs) {
        intervalMs_ = intervalMs;
    }

    void setCallback(std::function<void()> callback) {
        callback_ = std::move(callback);
        if (!timer_.isStarted()) {
            timer_.start();
        }
    }

private:
   uint32_t intervalMs_{};
   std::function<void()> callback_;
   Timer timer_;
   bool firstCall_ = true;
};

} // namespace QaplaHelpers
