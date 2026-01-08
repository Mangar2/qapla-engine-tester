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

std::vector<QaplaHelpers::IniFile::Section> AdjudicationConfig::getSections(
    const AdjudicationManager::DrawAdjudicationConfig& drawConfig,
    const AdjudicationManager::ResignAdjudicationConfig& resignConfig,
    const std::string& id) {
    
    std::vector<QaplaHelpers::IniFile::Section> sections;

    // Draw adjudication section
    QaplaHelpers::IniFile::KeyValueMap drawEntries{
        {"id", id},
        {"active", drawConfig.active ? "true" : "false"},
        {"minFullMoves", std::to_string(drawConfig.minFullMoves)},
        {"requiredConsecutiveMoves", std::to_string(drawConfig.requiredConsecutiveMoves)},
        {"centipawnThreshold", std::to_string(drawConfig.centipawnThreshold)},
        {"testOnly", drawConfig.testOnly ? "true" : "false"}
    };
    sections.push_back({
        .name = "drawadjudication",
        .entries = drawEntries
    });

    // Resign adjudication section
    QaplaHelpers::IniFile::KeyValueMap resignEntries{
        {"id", id},
        {"active", resignConfig.active ? "true" : "false"},
        {"requiredConsecutiveMoves", std::to_string(resignConfig.requiredConsecutiveMoves)},
        {"centipawnThreshold", std::to_string(resignConfig.centipawnThreshold)},
        {"twoSided", resignConfig.twoSided ? "true" : "false"},
        {"testOnly", resignConfig.testOnly ? "true" : "false"}
    };
    sections.push_back({
        .name = "resignadjudication",
        .entries = resignEntries
    });

    return sections;
}

void AdjudicationConfig::loadFromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& drawSections,
    const std::vector<QaplaHelpers::IniFile::Section>& resignSections,
    AdjudicationManager::DrawAdjudicationConfig& drawConfig,
    AdjudicationManager::ResignAdjudicationConfig& resignConfig) {
    
    // Load draw adjudication configuration
    if (!drawSections.empty()) {
        for (const auto& [key, value] : drawSections[0].entries) {
            if (key == "active") {
                drawConfig.active = (value == "true");
            }
            else if (key == "minFullMoves") {
                drawConfig.minFullMoves = QaplaHelpers::to_uint32(value).value_or(0);
            }
            else if (key == "requiredConsecutiveMoves") {
                drawConfig.requiredConsecutiveMoves = QaplaHelpers::to_uint32(value).value_or(0);
            }
            else if (key == "centipawnThreshold") {
                drawConfig.centipawnThreshold = QaplaHelpers::to_int(value).value_or(0);
            }
            else if (key == "testOnly") {
                drawConfig.testOnly = (value == "true");
            }
        }
    }

    // Load resign adjudication configuration
    if (!resignSections.empty()) {
        for (const auto& [key, value] : resignSections[0].entries) {
            if (key == "active") {
                resignConfig.active = (value == "true");
            }
            else if (key == "requiredConsecutiveMoves") {
                resignConfig.requiredConsecutiveMoves = QaplaHelpers::to_uint32(value).value_or(0);
            }
            else if (key == "centipawnThreshold") {
                resignConfig.centipawnThreshold = QaplaHelpers::to_int(value).value_or(0);
            }
            else if (key == "twoSided") {
                resignConfig.twoSided = (value == "true");
            }
            else if (key == "testOnly") {
                resignConfig.testOnly = (value == "true");
            }
        }
    }
}

void AdjudicationConfig::saveToConfigData(
    QaplaHelpers::ConfigData& configData,
    const AdjudicationManager::DrawAdjudicationConfig& drawConfig,
    const AdjudicationManager::ResignAdjudicationConfig& resignConfig,
    const std::string& id) {
    
    auto sections = getSections(drawConfig, resignConfig, id);
    configData.setSectionList("drawadjudication", id, { sections[0] });
    configData.setSectionList("resignadjudication", id, { sections[1] });
}

bool AdjudicationConfig::loadFromConfigData(
    const QaplaHelpers::ConfigData& configData,
    AdjudicationManager::DrawAdjudicationConfig& drawConfig,
    AdjudicationManager::ResignAdjudicationConfig& resignConfig,
    const std::string& id) {
    
    auto drawSections = configData.getSectionList("drawadjudication", id);
    auto resignSections = configData.getSectionList("resignadjudication", id);
    
    if ((!drawSections || drawSections->empty()) && 
        (!resignSections || resignSections->empty())) {
        return false;
    }

    loadFromSections(
        drawSections.value_or(std::vector<QaplaHelpers::IniFile::Section>{}),
        resignSections.value_or(std::vector<QaplaHelpers::IniFile::Section>{}),
        drawConfig,
        resignConfig);
    
    return true;
}

} // namespace QaplaTester
