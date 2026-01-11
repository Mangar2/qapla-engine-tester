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

#include "sprt-config-file.h"
#include "../base-elements/string-helper.h"
#include <algorithm>

namespace QaplaTester {

constexpr const char* SPRT_CONFIG_SECTION_NAME = "sprtconfig";

std::vector<QaplaHelpers::IniFile::Section> SprtConfigFile::getSections(
    const SprtConfig& config, const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"eloLower", std::to_string(config.eloLower)},
        {"eloUpper", std::to_string(config.eloUpper)},
        {"alpha", std::to_string(config.alpha)},
        {"beta", std::to_string(config.beta)},
        {"maxGames", std::to_string(config.maxGames)},
        {"model", config.model},
        {"pentanomial", config.pentanomial ? "true" : "false"}
    };

    return { QaplaHelpers::IniFile::Section{ .name = SPRT_CONFIG_SECTION_NAME, .entries = entries } };
}

SprtConfig SprtConfigFile::fromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& sections) {
    
    SprtConfig config;
    
    if (sections.empty()) {
        return config;
    }

    for (const auto& [key, value] : sections[0].entries) {
        if (key == "eloLower") {
            config.eloLower = QaplaHelpers::to_float(value).value_or(0.0F);
            config.eloLower = std::clamp(config.eloLower, -1000.0F, 1000.0F);
        }
        else if (key == "eloUpper") {
            config.eloUpper = QaplaHelpers::to_float(value).value_or(5.0F);
            config.eloUpper = std::clamp(config.eloUpper, -1000.0F, 1000.0F);
        }
        else if (key == "alpha") {
            config.alpha = QaplaHelpers::to_float(value).value_or(0.05);
            config.alpha = std::clamp(config.alpha, 0.001, 0.5);
        }
        else if (key == "beta") {
            config.beta = QaplaHelpers::to_float(value).value_or(0.05);
            config.beta = std::clamp(config.beta, 0.001, 0.5);
        }
        else if (key == "maxGames") {
            config.maxGames = QaplaHelpers::to_uint32(value).value_or(100000);
        }
        else if (key == "model") {
            config.model = value;
            if (config.model != "normalized" && config.model != "logistic" && config.model != "bayesian") {
                config.model = "normalized";
            }
        }
        else if (key == "pentanomial") {
            config.pentanomial = (value == "true" || value == "1");
        }
    }
    
    return config;
}

std::optional<SprtConfig> SprtConfigFile::fromConfigData(
    const QaplaHelpers::ConfigData& configData, 
    const std::string& id) {
    
    auto sections = configData.getSectionList(SPRT_CONFIG_SECTION_NAME, id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }

    return fromSections(*sections);
}

} // namespace QaplaTester
