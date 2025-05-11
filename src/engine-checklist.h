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

class EngineChecklist {
public:
    /**
     * Reports the result of a single evaluation point for a topic.
     *
     * @param topic Logical topic name (e.g., "Move legality")
     * @param passed True if the check passed; false if it failed.
     */
    static void report(std::string_view topic, bool passed) {
        auto& stat = stats_[std::string(topic)];
        ++stat.total;
        if (!passed) ++stat.failures;
    }

    static void print(std::ostream& os) {
        os << "== Engine Checklist Report ==\n";
        for (const auto& [topic, stat] : stats_) {
            const bool passed = stat.total > 0 && stat.failures == 0;
            const int percentFail = stat.total > 0
                ? static_cast<int>((100 * stat.failures) / stat.total)
                : 0;
            if (passed) {
                os << "PASS " << topic << " (" << stat.total << ")\n";
			}
            else {
                os << "FAIL " << topic << ": " << stat.failures << " failed / "
                    << stat.total << " total (" << percentFail << "% failure)\n";
            }
        }
    }

private:
    struct Stat {
        int total = 0;
        int failures = 0;
    };

    static inline std::unordered_map<std::string, Stat> stats_;
};
