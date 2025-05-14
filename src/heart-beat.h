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

#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>

class HeartBeat {
public:
    using Callback = std::function<void()>;

    explicit HeartBeat(Callback callback, std::chrono::milliseconds interval = std::chrono::seconds(1))
        : callback_(std::move(callback)), stop_(false) {
        thread_ = std::thread([this, interval]() {
            while (!stop_) {
                std::this_thread::sleep_for(interval);
                callback_();
            }
            });
    }

    ~HeartBeat() {
        stop_ = true;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    Callback callback_;
    std::atomic<bool> stop_;
    std::thread thread_;
};
