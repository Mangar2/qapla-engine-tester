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

#include <map>
#include <algorithm>
#include "checklist.h"

void Checklist::log() {
    Logger::testLogger().log("\n== Summary ==\n");

    enum class Section { Important, Missbehaviour, Notes };
    const std::unordered_map<std::string, Section> topicSections = {
        { "Start/Stop Engine", Section::Important },
        { "Engine Options works safely", Section::Important },
        { "No loss on time", Section::Important },
        { "Engine reacts on stop", Section::Important },
        { "wtime/btime overrun check", Section::Important },
        { "Computing a move returns a legal move", Section::Important },

        { "Infinite compute move must not exit on its own", Section::Missbehaviour },
        { "movetime overrun check", Section::Missbehaviour },
        { "PV check", Section::Missbehaviour },
		{ "currmove check", Section::Missbehaviour },
        { "Correct bestmove after immediate stop", Section::Missbehaviour },

        { "Hash Table Memory Adjustment", Section::Notes },
        { "movetime underrun check", Section::Notes },
        { "Uses enough time from time control", Section::Notes },
        { "Keeps reserve time", Section::Notes },
        { "Does not drop below 1s clock time", Section::Notes },
        { "search info unknown-token", Section::Notes }
    };

    std::map<Section, std::vector<std::pair<std::string, Stat>>> grouped;
    for (const auto& [topic, stat] : stats_) {
        auto section = topicSections.count(topic) ? topicSections.at(topic) : Section::Notes;
        grouped[section].emplace_back(topic, stat);
    }

    const std::map<Section, std::string> sectionTitles = {
        { Section::Important, "Important" },
        { Section::Missbehaviour, "Missbehaviour" },
        { Section::Notes, "Notes" }
    };

    for (const auto& [section, entries] : sectionTitles) {
        Logger::testLogger().log("[" + entries + "]");
        auto sorted = grouped[section];
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            const bool aFail = a.second.total > 0 && a.second.failures > 0;
            const bool bFail = b.second.total > 0 && b.second.failures > 0;
            return aFail > bFail;
            });

        size_t maxTopicLength = 0;
        for (const auto& [topic, _] : sorted) {
            maxTopicLength = std::max(maxTopicLength, topic.size());
        }

        bool lastWasFail = false;
        for (const auto& [topic, stat] : sorted) {
            const bool passed = stat.total > 0 && stat.failures == 0;
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
        Logger::testLogger().log("");
    }
}


