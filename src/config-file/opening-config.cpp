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

#include "opening-config.h"
#include "../base-elements/string-helper.h"

namespace QaplaTester {

std::vector<QaplaHelpers::IniFile::Section> OpeningConfig::getSections(
    const Openings& openings, const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"file", openings.file},
        {"order", openings.order},
        {"seed", std::to_string(openings.seed)},
        {"start", std::to_string(openings.start)},
        {"policy", openings.policy}
    };

    if (openings.plies) {
        entries.emplace_back("plies", std::to_string(*openings.plies));
    }

    return {{
        .name = "opening",
        .entries = entries
    }};
}

Openings OpeningConfig::loadFromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& sections) {
    
    Openings openings;
    
    if (sections.empty()) {
        return openings;
    }

    for (const auto& [key, value] : sections[0].entries) {
        if (key == "file") {
            openings.file = value;
        }
        else if (key == "order" && (value == "sequential" || value == "random")) {
            openings.order = value;
        }
        else if (key == "seed") {
            openings.seed = QaplaHelpers::to_uint32(value).value_or(815);
        }
        else if (key == "plies") {
            openings.plies = QaplaHelpers::to_int(value);
        }
        else if (key == "start") {
            openings.start = QaplaHelpers::to_uint32(value).value_or(0);
        }
        else if (key == "policy" && (value == "default" || value == "encounter" || value == "round")) {
            openings.policy = value;
        }
    }
    
    return openings;
}

std::optional<Openings> OpeningConfig::loadFromConfigData(
    const QaplaHelpers::ConfigData& configData, 
    const std::string& id) {
    
    auto sections = configData.getSectionList("opening", id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }

    return loadFromSections(*sections);
}

} // namespace QaplaTester
