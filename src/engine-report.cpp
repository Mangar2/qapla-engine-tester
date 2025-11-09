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

namespace QaplaTester {

void EngineReport::addTopic(const CheckTopic& topic) {
    std::scoped_lock lock(statsMutex_);
    auto it = std::ranges::find_if(registeredTopics_,
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

EngineReport::ReportData EngineReport::createReportData() {
    std::scoped_lock lock(statsMutex_);
    
    ReportData data;
    
    std::map<CheckSection, std::vector<std::pair<const CheckTopic*, CheckEntry>>> grouped;

    for (const auto& topic : registeredTopics_) {
        auto it = entries_.find(topic.id);
        if (it == entries_.end()) {
        continue;
    }
        grouped[topic.section].emplace_back(&topic, it->second);
    }

    // Helper lambda to process a section
    auto processSection = [](const std::vector<std::pair<const CheckTopic*, CheckEntry>>& items) -> std::vector<ReportLine> {
        std::vector<ReportLine> lines;
        
        // Sort items: failures first
        auto sortedItems = items;
        std::ranges::sort(sortedItems, [](const auto& a, const auto& b) {
            const bool aFail = a.second.total > 0 && a.second.failures > 0;
            const bool bFail = b.second.total > 0 && b.second.failures > 0;
            return aFail > bFail;
        });

        for (const auto& [topic, stat] : sortedItems) {
            const bool passed = stat.total > 0 && stat.failures == 0;
            ReportLine line;
            line.passed = passed;
            line.text = topic->text;
            line.failCount = passed ? 0 : stat.failures;
            lines.push_back(line);
        }
        
        return lines;
    };

    // Process each section
    data.important = processSection(grouped[CheckSection::Important]);
    data.missbehaviour = processSection(grouped[CheckSection::Missbehaviour]);
    data.notes = processSection(grouped[CheckSection::Notes]);
    data.report = processSection(grouped[CheckSection::Report]);

    return data;
}

AppReturnCode EngineReport::log(TraceLevel traceLevel, const std::optional<EngineResult>& engineResult) {
    AppReturnCode result = AppReturnCode::NoError;
    Logger::testLogger().log("\n== Summary ==\n", traceLevel);
    Logger::testLogger().log(engineName_ + (engineAuthor_.empty() ? "" : " by " + engineAuthor_) + "\n", traceLevel);

    ReportData data = createReportData();

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

    // Helper lambda to log a section
    auto logSection = [&](CheckSection section, const std::vector<ReportLine>& lines, const std::string& title) {
        if (section == CheckSection::Report) {
            if (engineResult) {
                Logger::testLogger().log("[" + title + "]", traceLevel);
                std::ostringstream oss;
                engineResult->printResults(oss);
                Logger::testLogger().log(oss.str(), traceLevel);
            }
            return;
        }

        Logger::testLogger().log("[" + title + "]", traceLevel);

        if (lines.empty()) {
            Logger::testLogger().log("", traceLevel);
            return;
        }

        // Calculate max topic length for formatting
        size_t maxTopicLength = 0;
        for (const auto& line : lines) {
            maxTopicLength = std::max(maxTopicLength, line.text.size());
        }

        logSections(lines, maxTopicLength, traceLevel, sectionCodes, section, result);

        Logger::testLogger().log("", traceLevel);
    };

    // Log all sections in order
    logSection(CheckSection::Important, data.important, sectionTitles.at(CheckSection::Important));
    logSection(CheckSection::Missbehaviour, data.missbehaviour, sectionTitles.at(CheckSection::Missbehaviour));
    logSection(CheckSection::Notes, data.notes, sectionTitles.at(CheckSection::Notes));
    logSection(CheckSection::Report, data.report, sectionTitles.at(CheckSection::Report));

    return result;
}

void EngineReport::logSections(const std::vector<ReportLine>& lines, size_t maxTopicLength, 
    TraceLevel traceLevel, const std::map<CheckSection, AppReturnCode>& sectionCodes, 
    CheckSection section, AppReturnCode& result) {
    bool lastWasFail = false;
    for (const auto& line : lines) {
        if (result == AppReturnCode::NoError && !line.passed) {
            result = sectionCodes.at(section);
        }
        if (line.passed && lastWasFail) {
            Logger::testLogger().log("", traceLevel);
        }
        std::ostringstream oss;
        oss << (line.passed ? "PASS " : "FAIL ");
        oss << std::left << std::setw(static_cast<int>(maxTopicLength) + 2) << line.text;
        if (!line.passed) {
            oss << "(" << line.failCount << " failed)";
        }
        lastWasFail = !line.passed;
        Logger::testLogger().log(oss.str(), traceLevel);
    }
}


const bool uciSearchInfoTopicsRegistered = [] {
    using enum EngineReport::CheckSection;

    EngineReport::addTopic({ .group="Stability", .id="no-disconnect", 
        .text="Engine does not disconnect during game", .section=Important });
    EngineReport::addTopic({ .group="Stability", .id="starts-and-stops-cleanly", 
        .text="Engine starts and stops quickly and without issues", .section=Important });
    EngineReport::addTopic({ .group="Stability", .id="reacts-on-stop", 
        .text="Engine handles 'stop' command reliably", .section=Important });
    EngineReport::addTopic({ .group="Stability", .id="infinite-move-does-not-exit", 
        .text="Infinite compute move does not terminate on its own", .section=Missbehaviour });

    EngineReport::addTopic({ .group="BestMove", .id="bestmove", 
        .text="Bestmove is followed by valid optional 'ponder' token", .section=Missbehaviour });
    EngineReport::addTopic({ .group="BestMove", .id="legalmove", 
        .text="Bestmove returned is a legal move", .section=Important });
    EngineReport::addTopic({ .group="BestMove", .id="correct-after-immediate-stop", 
        .text="Correct bestmove after immediate stop", .section=Missbehaviour });

    EngineReport::addTopic({ .group="Pondering", .id="legal-pondermove", 
        .text="Ponder move returned is a legal move", .section=Important });
    EngineReport::addTopic({ .group="Pondering", .id="correct-pondering", 
        .text="Correct pondering", .section=Important });

    EngineReport::addTopic({ .group="Time", .id="no-loss-on-time", 
        .text="Engine avoids time losses", .section=Important });
    EngineReport::addTopic({ .group="Time", .id="keeps-reserve-time", 
        .text="Engine preserves reserve time appropriately", .section=Notes });
    EngineReport::addTopic({ .group="Time", .id="not-below-one-second", 
        .text="Engine avoids dropping below 1 second on the clock", .section=Notes });

    EngineReport::addTopic({ .group="MoveTime", .id="supports-movetime", 
        .text="Supports movetime", .section=Notes });
    EngineReport::addTopic({ .group="MoveTime", .id="no-movetime-overrun", 
        .text="No movetime overrun", .section=Missbehaviour });
    EngineReport::addTopic({ .group="MoveTime", .id="no-movetime-underrun", 
        .text="No movetime underrun", .section=Notes });

    EngineReport::addTopic({ .group="DepthLimit", .id="supports-depth-limit", 
        .text="Supports depth limit", .section=Notes });
    EngineReport::addTopic({ .group="DepthLimit", .id="no-depth-overrun", 
        .text="No depth overrun", .section=Notes });
    EngineReport::addTopic({ .group="DepthLimit", .id="no-depth-underrun", 
        .text="No depth underrun", .section=Notes });

    EngineReport::addTopic({ .group="NodesLimit", .id="supports-node-limit",
        .text="Supports node limit", .section=Notes });
    EngineReport::addTopic({ .group="NodesLimit", .id="no-nodes-overrun", 
        .text="No nodes overrun", .section=Notes });
    EngineReport::addTopic({ .group="NodesLimit", .id="no-nodes-underrun", 
        .text="No nodes underrun", .section=Notes });

    EngineReport::addTopic({ .group="Tests", .id="shrinks-with-hash", 
        .text="Engine memory decreases when hash size is reduced", .section=Notes });
    EngineReport::addTopic({ .group="Tests", .id="options-safe", 
        .text="Engine options handling is safe and robust", .section=Important });

    EngineReport::addTopic({ .group="Score", .id="score cp", 
        .text="Search info reports correct score cp", .section=Missbehaviour });
    EngineReport::addTopic({ .group="Score", .id="score mate", 
        .text="Search info reports correct score mate", .section=Missbehaviour });

    EngineReport::addTopic({ .group="Depth", .id="depth", 
        .text="Search info reports correct depth", .section=Missbehaviour });
    EngineReport::addTopic({ .group="Depth", .id="seldepth", 
        .text="Search info reports correct selective depth", .section=Notes });

    EngineReport::addTopic({ .group="SearchInfo", .id="multipv", 
        .text="Search info reports correct multipv", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="time", 
        .text="Search info reports correct time", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="nodes", 
        .text="Search info reports correct nodes", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="nps", 
        .text="Search info reports correct nps", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="hashfull", 
        .text="Search info reports correct hashfull", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="tbhits", 
        .text="Search info reports correct tbhits", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="sbhits", 
        .text="Search info reports correct sbhits", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="cpuload", 
        .text="Search info reports correct cpuload", .section=Notes });

    EngineReport::addTopic({ .group="Currmove", .id="currmovenumber", 
        .text="Search info reports correct current move number", .section=Notes });
    EngineReport::addTopic({ .group="Currmove", .id="currmove", 
        .text="Search info reports correct current move", .section=Notes });

    EngineReport::addTopic({ .group="SearchInfo", .id="pv", 
        .text="Search info provides valid principal variation (PV)", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="duplicate-info-field", 
        .text="Search info field is reported more than once", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="unexpected-move-token", 
        .text="Unexpected move token in info line", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="wrong-token-in-info-line", 
        .text="Unrecognized or misplaced token in info line", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="parsing-exception", 
        .text="Parsing of search info threw an exception", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="lower-case-option", 
        .text="Engine accepts lower case option names", .section=Notes });

    // Winboard
    EngineReport::addTopic({ .group="SearchInfo", .id="missing-thinking-output", 
        .text="Engine provides all thinking output", .section=Notes });
    EngineReport::addTopic({ .group="SearchInfo", .id="no-engine-error-report", 
        .text="Engine did not report errors", .section=Notes });
    EngineReport::addTopic({ .group="Startup", .id="feature-report", 
        .text="Engine send features correctly", .section=Notes });

    EngineReport::addTopic({ .group="EPD", .id="epd-expected-moves", 
        .text="Simple EPD tests: expected moves found", .section=Notes });

    return true;
}();

} // namespace QaplaTester
