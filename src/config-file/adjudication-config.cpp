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

#include "adjudication-config.h"
#include "../base-elements/string-helper.h"

namespace QaplaTester {

QaplaHelpers::IniFile::Section AdjudicationConfig::toDrawSection(
    const AdjudicationManager::DrawAdjudicationConfig& config,
    const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"active", config.active ? "true" : "false"},
        {"minFullMoves", std::to_string(config.minFullMoves)},
        {"requiredConsecutiveMoves", std::to_string(config.requiredConsecutiveMoves)},
        {"centipawnThreshold", std::to_string(config.centipawnThreshold)},
        {"testOnly", config.testOnly ? "true" : "false"}
    };
    
    return {
        .name = "drawadjudication",
        .entries = entries
    };
}

QaplaHelpers::IniFile::Section AdjudicationConfig::toResignSection(
    const AdjudicationManager::ResignAdjudicationConfig& config,
    const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"active", config.active ? "true" : "false"},
        {"requiredConsecutiveMoves", std::to_string(config.requiredConsecutiveMoves)},
        {"centipawnThreshold", std::to_string(config.centipawnThreshold)},
        {"twoSided", config.twoSided ? "true" : "false"},
        {"testOnly", config.testOnly ? "true" : "false"}
    };
    
    return {
        .name = "resignadjudication",
        .entries = entries
    };
}

AdjudicationManager::DrawAdjudicationConfig AdjudicationConfig::fromDrawSection(
    const QaplaHelpers::IniFile::Section& section) {
    
    AdjudicationManager::DrawAdjudicationConfig config;
    
    for (const auto& [key, value] : section.entries) {
        if (key == "active") {
            config.active = (value == "true");
        }
        else if (key == "minFullMoves") {
            config.minFullMoves = QaplaHelpers::to_uint32(value).value_or(0);
        }
        else if (key == "requiredConsecutiveMoves") {
            config.requiredConsecutiveMoves = QaplaHelpers::to_uint32(value).value_or(0);
        }
        else if (key == "centipawnThreshold") {
            config.centipawnThreshold = QaplaHelpers::to_int(value).value_or(0);
        }
        else if (key == "testOnly") {
            config.testOnly = (value == "true");
        }
    }
    
    return config;
}

AdjudicationManager::ResignAdjudicationConfig AdjudicationConfig::fromResignSection(
    const QaplaHelpers::IniFile::Section& section) {
    
    AdjudicationManager::ResignAdjudicationConfig config;
    
    for (const auto& [key, value] : section.entries) {
        if (key == "active") {
            config.active = (value == "true");
        }
        else if (key == "requiredConsecutiveMoves") {
            config.requiredConsecutiveMoves = QaplaHelpers::to_uint32(value).value_or(0);
        }
        else if (key == "centipawnThreshold") {
            config.centipawnThreshold = QaplaHelpers::to_int(value).value_or(0);
        }
        else if (key == "twoSided") {
            config.twoSided = (value == "true");
        }
        else if (key == "testOnly") {
            config.testOnly = (value == "true");
        }
    }
    
    return config;
}

} // namespace QaplaTester
