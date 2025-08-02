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
 * @copyright Copyright (c) 2021 Volker Böhm
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
#include "engine-report.h"

void EngineReport::addTopic(const CheckTopic& topic) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    auto it = std::find_if(registeredTopics_.begin(), registeredTopics_.end(),
        [&](const CheckTopic& t) { return t.id == topic.id; });

    if (it != registeredTopics_.end()) {
        if (it->group != topic.group || it->text != topic.text || it->section != topic.section) {
            Logger::testLogger().log("Topic redefinition conflict for ID: " + topic.id, TraceLevel::error);
            throw std::runtime_error("Conflicting topic definition: " + topic.id);
        }
        return;
    }

    registeredTopics_.push_back(topic);
}

AppReturnCode EngineReport::logAll(TraceLevel traceLevel, const std::optional<TournamentResult>& result) {
    AppReturnCode worst = AppReturnCode::NoError;
    for (const auto& [name, checklist] : checklists_) {
        AppReturnCode code = checklist->log(traceLevel, result? result->forEngine(name) : std::nullopt);
		if (code == AppReturnCode::NoError) {
			continue;
		}
        if (worst == AppReturnCode::NoError || static_cast<int>(code) < static_cast<int>(worst)) {
            worst = code;
        }
    }
    return worst;
}

bool EngineReport::logReport(const std::string& topicId, bool passed, std::string_view detail,
    TraceLevel traceLevel) {
    report(topicId, passed);
    if (!passed) {
        const auto& entry = entries_[topicId];
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

AppReturnCode EngineReport::log(TraceLevel traceLevel, const std::optional<EngineResult>& engineResult) {
    AppReturnCode result = AppReturnCode::NoError;
    Logger::testLogger().log("\n== Summary ==\n", traceLevel);
    Logger::testLogger().log(engineName_ + (engineAuthor_.empty() ? "" : " by " + engineAuthor_) + "\n", traceLevel);

    std::map<CheckSection, std::vector<std::pair<const CheckTopic*, CheckEntry>>> grouped;

    for (const auto& topic : registeredTopics_) {
        auto it = entries_.find(topic.id);
        if (it == entries_.end()) continue;
        grouped[topic.section].emplace_back(&topic, it->second);
    }

    const std::map<CheckSection, std::string> sectionTitles = {
        { CheckSection::Important, "Important" },
        { CheckSection::Missbehaviour, "Missbehaviour" },
        { CheckSection::Notes, "Notes" },
        { CheckSection::Report, "Report" }
    };

    const std::map<CheckSection, AppReturnCode> sectionCodes = {
        { CheckSection::Important, AppReturnCode::EngineError },
        { CheckSection::Missbehaviour, AppReturnCode::EngineMissbehaviour },
        { CheckSection::Notes, AppReturnCode::EngineNote },
		{ CheckSection::Report, AppReturnCode::NoError  }
    };

    for (const auto& [section, entries] : sectionTitles) {

		if (section == CheckSection::Report) {
			if (engineResult) {
                Logger::testLogger().log("[" + entries + "]", traceLevel);
                std::ostringstream oss;
                engineResult->printResults(oss);
                Logger::testLogger().log(oss.str(), traceLevel);
			}
			continue;
		}

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
    using enum EngineReport::CheckSection;

    EngineReport::addTopic({ "Stability", "no-disconnect", "Engine does not disconnect during game", Important });
    EngineReport::addTopic({ "Stability", "starts-and-stops-cleanly", "Engine starts and stops quickly and without issues", Important });
    EngineReport::addTopic({ "Stability", "reacts-on-stop", "Engine handles 'stop' command reliably", Important });
    EngineReport::addTopic({ "Stability", "infinite-move-does-not-exit", "Infinite compute move does not terminate on its own", Missbehaviour });

    EngineReport::addTopic({ "BestMove", "bestmove", "Bestmove is followed by valid optional 'ponder' token", Missbehaviour });
    EngineReport::addTopic({ "BestMove", "legalmove", "Bestmove returned is a legal move", Important });
    EngineReport::addTopic({ "BestMove", "correct-after-immediate-stop", "Correct bestmove after immediate stop", Missbehaviour });

    EngineReport::addTopic({ "Pondering", "legal-pondermove", "Ponder move returned is a legal move", Important });
    EngineReport::addTopic({ "Pondering", "correct-pondering", "Correct pondering", Important });

    EngineReport::addTopic({ "Time", "no-loss-on-time", "Engine avoids time losses", Important });
    EngineReport::addTopic({ "Time", "keeps-reserve-time", "Engine preserves reserve time appropriately", Notes });
    EngineReport::addTopic({ "Time", "not-below-one-second", "Engine avoids dropping below 1 second on the clock", Notes });

    EngineReport::addTopic({ "MoveTime", "supports-movetime", "Supports movetime", Notes });
    EngineReport::addTopic({ "MoveTime", "no-movetime-overrun", "No movetime overrun", Missbehaviour });
    EngineReport::addTopic({ "MoveTime", "no-movetime-underrun", "No movetime underrun", Notes });

    EngineReport::addTopic({ "DepthLimit", "supports-depth-limit", "Supports depth limit", Notes });
    EngineReport::addTopic({ "DepthLimit", "no-depth-overrun", "No depth overrun", Notes });
    EngineReport::addTopic({ "DepthLimit", "no-depth-underrun", "No depth underrun", Notes });

    EngineReport::addTopic({ "NodesLimit", "supports-node-limit", "Supports node limit", Notes });
    EngineReport::addTopic({ "NodesLimit", "no-nodes-overrun", "No nodes overrun", Notes });
    EngineReport::addTopic({ "NodesLimit", "no-nodes-underrun", "No nodes underrun", Notes });

    EngineReport::addTopic({ "Tests", "shrinks-with-hash", "Engine memory decreases when hash size is reduced", Notes });
    EngineReport::addTopic({ "Tests", "options-safe", "Engine options handling is safe and robust", Important });

    EngineReport::addTopic({ "Score", "score cp", "Search info reports correct score cp", Missbehaviour });
    EngineReport::addTopic({ "Score", "score mate", "Search info reports correct score mate", Missbehaviour });

    EngineReport::addTopic({ "Depth", "depth", "Search info reports correct depth", Missbehaviour });
    EngineReport::addTopic({ "Depth", "seldepth", "Search info reports correct selective depth", Notes });

    EngineReport::addTopic({ "SearchInfo", "multipv", "Search info reports correct multipv", Notes });
    EngineReport::addTopic({ "SearchInfo", "time", "Search info reports correct time", Notes });
    EngineReport::addTopic({ "SearchInfo", "nodes", "Search info reports correct nodes", Notes });
    EngineReport::addTopic({ "SearchInfo", "nps", "Search info reports correct nps", Notes });
    EngineReport::addTopic({ "SearchInfo", "hashfull", "Search info reports correct hashfull", Notes });
    EngineReport::addTopic({ "SearchInfo", "tbhits", "Search info reports correct tbhits", Notes });
    EngineReport::addTopic({ "SearchInfo", "sbhits", "Search info reports correct sbhits", Notes });
    EngineReport::addTopic({ "SearchInfo", "cpuload", "Search info reports correct cpuload", Notes });

    EngineReport::addTopic({ "Currmove", "currmovenumber", "Search info reports correct current move number", Notes });
    EngineReport::addTopic({ "Currmove", "currmove", "Search info reports correct current move", Notes });

    EngineReport::addTopic({ "SearchInfo", "pv", "Search info provides valid principal variation (PV)", Notes });
    EngineReport::addTopic({ "SearchInfo", "duplicate-info-field", "Search info field is reported more than once", Notes });
    EngineReport::addTopic({ "SearchInfo", "unexpected-move-token", "Unexpected move token in info line", Notes });
    EngineReport::addTopic({ "SearchInfo", "wrong-token-in-info-line", "Unrecognized or misplaced token in info line", Notes });
    EngineReport::addTopic({ "SearchInfo", "parsing-exception", "Parsing of search info threw an exception", Notes });
	EngineReport::addTopic({ "SearchInfo", "lower-case-option", "Engine accepts lower case option names", Notes });
    
    // Winboard
    EngineReport::addTopic({ "SearchInfo", "missing-thinking-output", "Engine provides all thinking output", Notes });
    EngineReport::addTopic({ "SearchInfo", "no-engine-error-report", "Engine did not report errors", Notes });
    EngineReport::addTopic({ "Startup", "feature-report", "Engine send features correctly", Notes });
    
    EngineReport::addTopic({ "EPD", "epd-expected-moves", "Simple EPD tests: expected moves found", Notes });

    return true;
}();


