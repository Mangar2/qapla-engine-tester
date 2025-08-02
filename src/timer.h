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
    }

	uint64_t elapsedMs(uint64_t end) const {
		return end - start_;
	}
    uint64_t elapsedMs() const {
        return getCurrentTimeMs() - start_;
    }

    void printElapsed(const char* label) {
		uint64_t elapsed = elapsedMs();
        int sec = (elapsed / 1000) % 60;
        
        std::cout << "[Timer] " << label << ": elapsed = " 
            << std::right 
			<< elapsed / 1000 / 60 << ":" 
			<< std::setw(2) << std::setfill('0') << sec << "." 
            << std::setw(3) << std::setfill('0') << elapsed % 1000 << std::endl;
    }

private:

    uint64_t start_{};
};

