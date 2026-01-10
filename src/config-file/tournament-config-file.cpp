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

#include "tournament-config-file.h"
#include "../base-elements/string-helper.h"

namespace QaplaTester {

std::vector<QaplaHelpers::IniFile::Section> TournamentConfigFile::getSections(
    const TournamentConfig& config, const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"event", config.event},
        {"type", config.type},
        {"rounds", std::to_string(config.rounds)},
        {"games", std::to_string(config.games)},
        {"repeat", std::to_string(config.repeat)},
        {"noSwap", config.noSwap ? "true" : "false"},
        {"averageElo", std::to_string(config.averageElo)},
        {"saveInterval", std::to_string(config.saveInterval)}
    };

    return { QaplaHelpers::IniFile::Section{ .name = "tournament", .entries = entries } };
}

TournamentConfig TournamentConfigFile::fromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& sections) {
    
    TournamentConfig config;
    
    if (sections.empty()) {
        return config;
    }

    for (const auto& [key, value] : sections[0].entries) {
        if (key == "event") {
            config.event = value;
        }
        else if (key == "type") {
            config.type = value;
        }
        else if (key == "rounds") {
            config.rounds = QaplaHelpers::to_uint32(value).value_or(1);
        }
        else if (key == "games") {
            config.games = QaplaHelpers::to_uint32(value).value_or(1);
        }
        else if (key == "repeat") {
            config.repeat = QaplaHelpers::to_uint32(value).value_or(1);
        }
        else if (key == "noSwap") {
            config.noSwap = (value == "true");
        }
        else if (key == "averageElo") {
            config.averageElo = QaplaHelpers::to_int(value).value_or(0);
        }
        else if (key == "saveInterval") {
            config.saveInterval = QaplaHelpers::to_uint32(value).value_or(10);
        }
    }
    
    return config;
}

std::optional<TournamentConfig> TournamentConfigFile::fromConfigData(
    const QaplaHelpers::ConfigData& configData, 
    const std::string& id) {
    
    auto sections = configData.getSectionList("tournament", id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }

    return fromSections(*sections);
}

} // namespace QaplaTester
