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

#include <map>
#include <array>
#include <algorithm>
#include "checklist.h"

void Checklist::addTopic(const CheckTopic& topic) {
    std::lock_guard lock(statsMutex_);
    auto [it, inserted] = knownTopics_.emplace(topic.id, topic);
    if (!inserted) {
        const auto& existing = it->second;
        if (existing.group != topic.group ||
            existing.text != topic.text ||
            existing.section != topic.section) {
            Logger::testLogger().log("Topic redefinition conflict for ID: " + topic.id, TraceLevel::error);
            throw std::runtime_error("Conflicting topic definition: " + topic.id);
        }
    }
}

AppReturnCode Checklist::logAll(TraceLevel traceLevel) {
    AppReturnCode worst = AppReturnCode::NoError;
    for (const auto& [name, checklist] : checklists_) {
        AppReturnCode code = checklist->log(traceLevel);
		if (code == AppReturnCode::NoError) {
			continue;
		}
        if (static_cast<int>(code) < static_cast<int>(worst)) {
            worst = code;
        }
    }
    return worst;
}

bool Checklist::logReport(const std::string& topicId, bool passed, std::string_view detail,
    TraceLevel traceLevel) {
    report(topicId, passed);
    if (!passed) {
        const auto& entry = stats_[topicId];
        if (entry.failures > MAX_CLI_LOGS_PER_ERROR && traceLevel < TraceLevel::error) {
            return false;
        }
        Logger::testLogger().log("\n[Report for topic \"" + topicId + "\"] " + std::string(detail),
            entry.failures > MAX_CLI_LOGS_PER_ERROR ? TraceLevel::info : traceLevel);
        if (entry.failures == MAX_CLI_LOGS_PER_ERROR) {
            Logger::testLogger().log("Too many similar reports. Further reports of this type will be suppressed.", traceLevel);
        }
    }
    return passed;
}




void Checklist::addMissingTopicsAsFail() {
    const std::vector<std::string> missingSearchInfoTopics = {
        "Search info reports correct depth",
        "Search info reports correct selective depth",
        "Search info reports correct multipv",
        "Search info reports correct score",
        "Search info reports correct time",
        "Search info reports correct nodes",
        "Search info reports correct nps",
        "Search info reports correct hashfull",
        "Search info reports correct cpuload",
        "Search info reports correct move number",
        "Search info reports correct current move",
        "Search info reports correct PV"
    };

    for (const auto& topic : missingSearchInfoTopics) {
        if (!stats_.contains(topic)) {
            reportOld(topic, true);
        }
    }
}

AppReturnCode Checklist::logOld(TraceLevel traceLevel) {
	AppReturnCode code = AppReturnCode::NoError;
    Logger::testLogger().log("\n== Summary ==\n");
	Logger::testLogger().log(name_ + " by " + author_ + "\n");

    enum class Section { Important = 0, Missbehaviour = 1, Notes = 2};
	std::array<AppReturnCode, 3> sectionCodes = {
		AppReturnCode::EngineError, // Important
		AppReturnCode::EngineMissbehaviour, // Missbehaviour
		AppReturnCode::EngineNote // Notes
	};

    const std::unordered_map<std::string, Section> topicSections = {
        { "Engine starts and stops fast and without problems", Section::Important },
        { "Engine Options works safely", Section::Important },
        { "No loss on time", Section::Important },
        { "No disconnect", Section::Important },
        { "Engine reacts on stop", Section::Important },
        { "Computing a move returns a legal move", Section::Important },
        { "Correct pondering", Section::Important },

        { "Infinite compute move must not exit on its own", Section::Missbehaviour },
        { "No movetime overrun", Section::Missbehaviour },
        { "Search info reports correct PV", Section::Missbehaviour },
		{ "Search info reports correct current move", Section::Missbehaviour },
        { "Correct bestmove after immediate stop", Section::Missbehaviour },
        { "Ponder move is legal", Section::Missbehaviour }
    };

	addMissingTopicsAsFail();

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
        Logger::testLogger().log("[" + entries + "]", traceLevel);
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
			if (code == AppReturnCode::NoError && !passed) {
				code = sectionCodes[static_cast<int>(section)];
			}
            if (passed && lastWasFail) {
				// An empty line between fail and pass topics
                Logger::testLogger().log("", traceLevel);
            }
            std::ostringstream line;
            line << (passed ? "PASS " : "FAIL ");
            line << std::left << std::setw(static_cast<int>(maxTopicLength) + 2) << topic;
            if (!passed) {
                line << "(" << stat.failures << " failed)";
            }
            lastWasFail = !passed;
            Logger::testLogger().log(line.str(), traceLevel);
        }
        Logger::testLogger().log("", traceLevel);
    }
    return code;
}

AppReturnCode Checklist::log(TraceLevel traceLevel) {
    AppReturnCode result = AppReturnCode::NoError;
    Logger::testLogger().log("\n== Summary ==\n");
    Logger::testLogger().log(engineName_ + " by " + engineAuthor_ + "\n");

    std::map<CheckSection, std::vector<std::pair<const CheckTopic*, CheckEntry>>> grouped;

    for (const auto& [topicId, entry] : entries_) {
        auto it = knownTopics_.find(topicId);
        if (it != knownTopics_.end()) {
            const CheckTopic& topic = it->second;
            grouped[topic.section].emplace_back(&topic, entry);
        }
        else {
            Logger::testLogger().log("[Unknown topic: " + topicId + "]", traceLevel);
        }
    }

    const std::map<CheckSection, std::string> sectionTitles = {
        { CheckSection::Important, "Important" },
        { CheckSection::Missbehaviour, "Missbehaviour" },
        { CheckSection::Notes, "Notes" }
    };

    const std::map<CheckSection, AppReturnCode> sectionCodes = {
        { CheckSection::Important, AppReturnCode::EngineError },
        { CheckSection::Missbehaviour, AppReturnCode::EngineMissbehaviour },
        { CheckSection::Notes, AppReturnCode::EngineNote },
		{ CheckSection::Report, AppReturnCode::NoError  }
    };

    for (const auto& [section, entries] : sectionTitles) {
        Logger::testLogger().log("[" + entries + "]", traceLevel);

        auto& items = grouped[section];
        std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
            const bool aFail = a.second.total > 0 && a.second.failures > 0;
            const bool bFail = b.second.total > 0 && b.second.failures > 0;
            return aFail > bFail;
            });

        size_t maxTopicLength = 0;
        for (const auto& [topic, _] : items) {
            maxTopicLength = std::max(maxTopicLength, topic->text.size());
        }

        bool lastWasFail = false;
        for (const auto& [topic, stat] : items) {
            const bool passed = stat.total > 0 && stat.failures == 0;
            if (result == AppReturnCode::NoError && !passed) {
                result = sectionCodes.at(section);
            }
            if (passed && lastWasFail) {
                Logger::testLogger().log("", traceLevel);
            }
            std::ostringstream line;
            line << (passed ? "PASS " : "FAIL ");
            line << std::left << std::setw(static_cast<int>(maxTopicLength) + 2) << topic->text;
            if (!passed) {
                line << "(" << stat.failures << " failed)";
            }
            lastWasFail = !passed;
            Logger::testLogger().log(line.str(), traceLevel);
        }

        Logger::testLogger().log("", traceLevel);
    }

    return result;
}

const bool uciSearchInfoTopicsRegistered = [] {
    using enum Checklist::CheckSection;
    // Common bounded int/64 topics (readBoundedInt)
    Checklist::addTopic({ "SearchInfo", "score cp", "Search info reports correct score cp", Missbehaviour });
    Checklist::addTopic({ "SearchInfo", "score mate", "Search info reports correct score mate", Missbehaviour });
    Checklist::addTopic({ "SearchInfo", "depth", "Search info reports correct depth", Missbehaviour });
    Checklist::addTopic({ "SearchInfo", "seldepth", "Search info reports correct selective depth", Notes });
    Checklist::addTopic({ "SearchInfo", "multipv", "Search info reports correct multipv", Notes });
    Checklist::addTopic({ "SearchInfo", "time", "Search info reports correct time", Notes });
    Checklist::addTopic({ "SearchInfo", "nodes", "Search info reports correct nodes", Notes });
    Checklist::addTopic({ "SearchInfo", "nps", "Search info reports correct nps", Notes });
    Checklist::addTopic({ "SearchInfo", "hashfull", "Search info reports correct hashfull", Notes });
    Checklist::addTopic({ "SearchInfo", "tbhits", "Search info reports correct tbhits", Notes });
    Checklist::addTopic({ "SearchInfo", "sbhits", "Search info reports correct sbhits", Notes });
    Checklist::addTopic({ "SearchInfo", "cpuload", "Search info reports correct cpuload", Notes });
    Checklist::addTopic({ "SearchInfo", "currmovenumber", "Search info reports correct current move number", Notes });

    // Structural or semantic info parsing issues
    Checklist::addTopic({ "SearchInfo", "duplicate-info-field", "Search info field reported multiple times", Notes });
    Checklist::addTopic({ "SearchInfo", "unexpected-move-token", "Unexpected move token in info line", Notes });
    Checklist::addTopic({ "SearchInfo", "wrong-token-in-info-line", "Unrecognized token in info line", Notes });
    Checklist::addTopic({ "SearchInfo", "parsing-exception", "Search info parsing threw exception", Notes });

    Checklist::addTopic({ "BestMove", "bestmove", "Bestmove is followed by correct optional 'ponder' token", Missbehaviour });
    Checklist::addTopic({ "SearchInfo", "currmove", "Search info reports correct current move", Notes });
    Checklist::addTopic({ "SearchInfo", "pv", "Search info reports correct PV", Missbehaviour });
    Checklist::addTopic({ "BestMove", "legalmove", "Computing a move returns a legal move", Important });
    Checklist::addTopic({ "Time", "no-loss-on-time", "No loss on time", Important });
    Checklist::addTopic({ "Time", "no-movetime-overrun", "No movetime overrun", Missbehaviour });
    Checklist::addTopic({ "Time", "no-movetime-underrun", "No movetime underrun", Notes });
    Checklist::addTopic({ "SearchInfo", "no-depth-overrun", "No depth overrun", Notes });
    Checklist::addTopic({ "SearchInfo", "no-depth-underrun", "No depth underrun", Notes });
    Checklist::addTopic({ "SearchInfo", "no-nodes-overrun", "No nodes overrun", Notes });
    Checklist::addTopic({ "SearchInfo", "no-nodes-underrun", "No nodes underrun", Notes });
    Checklist::addTopic({ "Stability", "no-disconnect", "No disconnect", Important });
    Checklist::addTopic({ "Pondering", "legal-pondermove", "Ponder move is legal", Important });



    return true;
    }();
    
