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
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Implements a list holding moves of a chess position
 * Moves are stored in one list - but different for "silent moves" and "non silent moves". Silent moves are moves 
 * not capturing and not promoting - non silent moves are captures and promotes.
 * Silent moves are pushed to the end and non silent moves are inserted to the front. As a result non silent moves
 * are always ordered first
 */

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include "logger.h"

class EngineChecklist {
public:
    /**
     * Reports the result of a single evaluation point for a topic.
     *
     * @param topic Logical topic name (e.g., "Move legality")
     * @param passed True if the check passed; false if it failed.
     */
    static void report(const std::string topic, bool passed) {
        auto& stat = stats_[topic];
        ++stat.total;
        if (!passed) ++stat.failures;
    }

    /**
	 * @brief Returns the number of errors for a given topic.
	 * @param topic The topic to check.
     */
    static int getNumErrors(const std::string& topic) {
		auto& stat = stats_[topic];
		return stat.failures;
    }

    /**
	 * @brief Logs the results of all tests to the test logger.
     */
    static void log() {
        Logger::testLogger().log("\n== Engine Report ==\n");

        std::vector<std::pair<std::string, Stat>> sorted(stats_.begin(), stats_.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            const bool aFail = a.second.total > 0 && a.second.failures > 0;
            const bool bFail = b.second.total > 0 && b.second.failures > 0;
            return aFail > bFail; 
            });

        size_t maxTopicLength = 0;
        for (const auto& [topic, _] : sorted) {
            maxTopicLength = std::max(maxTopicLength, std::string(topic).size());
        }
        bool lastWasFail = false;
        for (const auto& [topic, stat] : sorted) {
            const bool passed = stat.total > 0 && stat.failures == 0;
            const int percentFail = stat.total > 0
                ? static_cast<int>((100 * stat.failures) / stat.total)
                : 0;
			if (passed && lastWasFail) {
				Logger::testLogger().log("");
			}
            std::ostringstream line;
            line << (passed ? "PASS " : "FAIL ");
            line << std::left << std::setw(static_cast<int>(maxTopicLength) + 2) << topic;
            if (!passed) {
                line << "(" << stat.failures << " failed)";
            }

            lastWasFail = !passed;
            Logger::testLogger().log(line.str());
        }
    }


private:
    struct Stat {
        int total = 0;
        int failures = 0;
    };

    static inline std::unordered_map<std::string, Stat> stats_;
};
