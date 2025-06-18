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

	EngineDuelResult switchedSides() const;

	std::string toString() const {
        std::ostringstream oss;
        oss << engineA << " vs " << engineB
			<< std::fixed << std::setprecision(2)
            << " ( " << engineARate() << " ) "
            << " W:" << winsEngineA << " D:" << draws << " L:" << winsEngineB;
		return oss.str();
	}

	std::string toResultString() const {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2)
			<< " ( " << engineARate() << " ) "
			<< " W:" << winsEngineA << " D:" << draws << " L:" << winsEngineB;
		return oss.str();
	}

	EngineDuelResult& operator+=(const EngineDuelResult& other);
};

/**
 * @brief Holds all duel results of one engine and computes an aggregate over them.
 */
struct EngineResult {
	std::vector<EngineDuelResult> duels;
	std::string engineName;

	/**
	 * @brief Returns a single aggregated result across all duels.
	 *        engineA is set, engineB is empty.
	 * @param targetEngine The name of the engine to aggregate results for.
	 */
	EngineDuelResult aggregate(const std::string& targetEngine) const;


	void writeTo(std::ostream& os) const;

};

/**
 * @brief Collects duel results between engines and provides aggregated statistics per engine.
 *        Used to analyze tournament-level performance data.
 */
class TournamentResult {
public:
	/**
	 * @brief Adds a single EngineDuelResult to the internal collection.
	 *        Can include matches between any engine pair.
	 * @param result A duel result to include.
	 */
	void add(const EngineDuelResult& result);

	/**
	 * @brief Returns the names of all engines for which results have been recorded.
	 * @return A vector of unique engine names.
	 */
	std::vector<std::string> engineNames() const;

	/**
	 * @brief Computes and returns all duel results for the given engine.
	 * @param name The engine name.
	 * @return An EngineResult object with individual duels and aggregate data, or std::nullopt if unknown.
	 */
	std::optional<EngineResult> forEngine(const std::string& name) const;

private:
	std::vector<EngineDuelResult> results_;
};