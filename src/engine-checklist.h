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

 /**
  * Represents a single checklist item for engine evaluation.
  */
struct EngineChecklistItem {
    std::string category;
    std::string description;
    bool passed;
};

/**
 * Holds and outputs the structured checklist result of the engine.
 */
class EngineChecklist {
public:
    static void addItem(const std::string& category, const std::string& description, bool passed) {
        items_.emplace_back(EngineChecklistItem{ category, description, passed });
    }

    static void print(std::ostream& os) {
        os << "== Engine Checklist Report ==\n";
        for (const auto& item : items_) {
            os << (item.passed ? "PASS " : "FAIL ")  
                << item.description << " (" << item.category << ")\n";
        }
    }

private:
    static inline std::vector<EngineChecklistItem> items_;
};