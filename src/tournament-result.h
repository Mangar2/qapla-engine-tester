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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#pragma once

#include <vector>
#include <optional>
#include <string>
#include <istream>
#include <ostream>
#include <iomanip>
#include <mutex>
#include <array>
#include <sstream>

#include "game-result.h"
#include "game-record.h"


struct EngineDuelResult {
    std::string engineA;
    std::string engineB;
    int winsEngineA = 0;
    int winsEngineB = 0;
    int draws = 0;
    std::array<int, static_cast<size_t>(GameEndCause::Count)> causeCounters{};

	void clear() {
		winsEngineA = 0;
		winsEngineB = 0;
		draws = 0;
		causeCounters.fill(0);
	}
    
    double engineARate() const {
		double totalGames = winsEngineA + winsEngineB + draws;
		return totalGames > 0 ? (winsEngineA * 1.0 + draws * 0.5)  / totalGames : 0.0;
    }
	double engineBRate() const {
		double totalGames = winsEngineA + winsEngineB + draws;
		return totalGames > 0 ? (winsEngineB * 1.0 + draws * 0.5) / totalGames : 0.0;
	}
	void addResult(const GameRecord& record) {
        bool whiteIsEngineA = engineA == record.getWhiteEngineName();
		auto [cause, result] = record.getGameResult();
		if (result == GameResult::Draw) {
			++draws;
		}
		if (result == GameResult::WhiteWins) {
			winsEngineA += whiteIsEngineA;
			winsEngineB += !whiteIsEngineA;
		}
		else if (result == GameResult::BlackWins) {
			winsEngineB += whiteIsEngineA;
			winsEngineA += !whiteIsEngineA;
		}
		++causeCounters[static_cast<size_t>(cause)];
	}

	std::string toString() const {
        std::ostringstream oss;
        oss << engineA << " vs " << engineB
			<< std::fixed << std::setprecision(2)
            << " ( " << engineARate() << " ) "
            << " W:" << winsEngineA << " D:" << draws << " L:" << winsEngineB;
		return oss.str();
	}
};
