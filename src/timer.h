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
    static int64_t getCurrentTimeMs() {
        return duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void start() {
        start_ = getCurrentTimeMs();
    }

	int64_t elapsedMs(int64_t end) const {
		return end - start_;
	}
    int64_t elapsedMs() const {
        return getCurrentTimeMs() - start_;
    }

    void printElapsed(const char* label) {
		int64_t elapsed = elapsedMs();
        int sec = (elapsed / 1000) % 60;
        
        std::cout << "[Timer] " << label << ": elapsed = " 
            << std::right 
			<< elapsed / 1000 / 60 << ":" // minutes
			<< std::setw(2) << std::setfill('0')
			<< ((elapsed / 1000) % 60) << "." // seconds
			<< std::setw(3) << std::setfill('0')
            << elapsed % 1000 << std::endl;
    }

private:

    int64_t start_{};
};

