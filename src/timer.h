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

#include <sys/timeb.h>
#include <iostream>
#include <chrono>

 class Timer {
public:
    static int64_t getCurrentTimeMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    void start() {
        start_ = getCurrentTimeMs();
    }

    void stop() {
		end_ = getCurrentTimeMs();
    }

	int64_t elapsedMs(int64_t end) const {
		return end - start_;
	}
    int64_t elapsedMs() const {
        return end_ - start_;
    }

    void printElapsed(const char* label) {
		int64_t cur = getCurrentTimeMs();
		int64_t elapsed = cur - start_;
        std::cout << "[Timer] " << label << ": elapsed = " << elapsed << " ms" << std::endl;
    }

private:

    int64_t start_{};
    int64_t end_{};
};

