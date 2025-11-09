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

#include <iostream>
#include <chrono>
#include <iomanip>

namespace QaplaHelpers {

class Timer {
public:
    
    static uint64_t getCurrentTimeMs() {
        return static_cast<uint64_t>(duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    
    static uint64_t getSystemTimeMs() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count());
    }

    void start() {
        start_ = getCurrentTimeMs();
        started_ = true;
    }

    [[nodiscard]] bool isStarted() const {
        return started_;
    }

    void stop() {
        if (!started_) {
            return;
        }
        started_ = false;
        end_ = getCurrentTimeMs();
    }

    void reset() {
        started_ = false;
        start_ = 0;
        end_ = 0;
    }

	[[nodiscard]] uint64_t elapsedMs(uint64_t end) const {
		return end - start_;
	}
    [[nodiscard]] uint64_t elapsedMs() const {
        return started_ ? getCurrentTimeMs() - start_ : end_ - start_;
    }

    void printElapsed(const char* label) const {
		uint64_t elapsed = elapsedMs();
        int sec = (elapsed / 1000) % 60;
        
        std::cout << "[Timer] " << label << ": elapsed = " 
            << std::right 
			<< elapsed / 1000 / 60 << ":" 
			<< std::setw(2) << std::setfill('0') << sec << "." 
            << std::setw(3) << std::setfill('0') << elapsed % 1000 << "\n" << std::flush;
    }

private:
    bool started_ = false;
    uint64_t start_{};
    uint64_t end_{};
};

} // namespace QaplaHelpers