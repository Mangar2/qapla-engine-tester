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

#include "pgn-config.h"

namespace QaplaTester {

std::vector<QaplaHelpers::IniFile::Section> PgnConfig::toSections(
    const PgnSave::Options& options, const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"file", options.file},
        {"append", options.append ? "true" : "false"},
        {"onlyFinishedGames", options.onlyFinishedGames ? "true" : "false"},
        {"minimalTags", options.minimalTags ? "true" : "false"},
        {"saveAfterMove", options.saveAfterMove ? "true" : "false"},
        {"includeClock", options.includeClock ? "true" : "false"},
        {"includeEval", options.includeEval ? "true" : "false"},
        {"includePv", options.includePv ? "true" : "false"},
        {"includeDepth", options.includeDepth ? "true" : "false"}
    };

    return {{
        .name = getSectionName(),
        .entries = entries
    }};
}

PgnSave::Options PgnConfig::fromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& sections) {
    
    PgnSave::Options options;
    
    if (sections.empty()) {
        return options;
    }

    for (const auto& [key, value] : sections[0].entries) {
        if (key == "file") {
            options.file = value;
        }
        else if (key == "append") {
            options.append = (value == "true");
        }
        else if (key == "onlyFinishedGames") {
            options.onlyFinishedGames = (value == "true");
        }
        else if (key == "minimalTags") {
            options.minimalTags = (value == "true");
        }
        else if (key == "saveAfterMove") {
            options.saveAfterMove = (value == "true");
        }
        else if (key == "includeClock") {
            options.includeClock = (value == "true");
        }
        else if (key == "includeEval") {
            options.includeEval = (value == "true");
        }
        else if (key == "includePv") {
            options.includePv = (value == "true");
        }
        else if (key == "includeDepth") {
            options.includeDepth = (value == "true");
        }
    }
    
    return options;
}

std::optional<PgnSave::Options> PgnConfig::fromConfigData(
    const QaplaHelpers::ConfigData& configData, 
    const std::string& id) {
    
    auto sections = configData.getSectionList(getSectionName(), id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }

    return fromSections(*sections);
}

PgnSave::Options PgnConfig::fromManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto pgnInstance = manager.getGroupInstance(groupName);
    if (!pgnInstance) {
        return PgnSave::Options{};
    }

    const auto& pgn = *pgnInstance;
    return PgnSave::Options{
        .file = pgn.get<std::string>("file"),
        .append = pgn.get<bool>("append"),
        .onlyFinishedGames = pgn.get<bool>("finished"),
        .minimalTags = pgn.get<bool>("min"),
        .includeClock = pgn.get<bool>("clock"),
        .includeEval = pgn.get<bool>("eval"),
        .includePv = pgn.get<bool>("pv"),
        .includeDepth = pgn.get<bool>("depth")
    };
}

} // namespace QaplaTester
