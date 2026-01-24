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

std::vector<QaplaHelpers::IniFile::Section> AdjudicationConfig::toDrawSections(
    const AdjudicationManager::DrawAdjudicationConfig& config,
    const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"active", config.active ? "true" : "false"},
        {"movenumber", std::to_string(config.minFullMoves)},
        {"movecount", std::to_string(config.requiredConsecutiveMoves)},
        {"score", std::to_string(config.centipawnThreshold)},
        {"testOnly", config.testOnly ? "true" : "false"}
    };
    
    return {{
        .name = getDrawSectionName(),
        .entries = entries
    }};
}

std::vector<QaplaHelpers::IniFile::Section> AdjudicationConfig::toResignSections(
    const AdjudicationManager::ResignAdjudicationConfig& config,
    const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"active", config.active ? "true" : "false"},
        {"movecount", std::to_string(config.requiredConsecutiveMoves)},
        {"score", std::to_string(config.centipawnThreshold)},
        {"twoSided", config.twoSided ? "true" : "false"},
        {"testOnly", config.testOnly ? "true" : "false"}
    };
    
    return {{
        .name = getResignSectionName(),
        .entries = entries
    }};
}

std::optional<AdjudicationManager::DrawAdjudicationConfig> AdjudicationConfig::fromDrawConfigData(
    const QaplaHelpers::ConfigData& configData,
    const std::string& id) {
    
    auto sections = configData.getSectionList(getDrawSectionName(), id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }
    
    const auto& section = (*sections)[0];
    AdjudicationManager::DrawAdjudicationConfig config;
    
    for (const auto& [key, value] : section.entries) {
        if (key == "active") {
            config.active = (value == "true");
        }
        else if (key == "movenumber") {
            config.minFullMoves = QaplaHelpers::to_uint32(value).value_or(0);
        }
        else if (key == "movecount") {
            config.requiredConsecutiveMoves = QaplaHelpers::to_uint32(value).value_or(0);
        }
        else if (key == "score") {
            config.centipawnThreshold = QaplaHelpers::to_int(value).value_or(0);
        }
        else if (key == "testOnly") {
            config.testOnly = (value == "true");
        }
    }
    
    return config;
}

std::optional<AdjudicationManager::ResignAdjudicationConfig> AdjudicationConfig::fromResignConfigData(
    const QaplaHelpers::ConfigData& configData,
    const std::string& id) {
    
    auto sections = configData.getSectionList(getResignSectionName(), id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }
    
    const auto& section = (*sections)[0];
    AdjudicationManager::ResignAdjudicationConfig config;
    
    for (const auto& [key, value] : section.entries) {
        if (key == "active") {
            config.active = (value == "true");
        }
        else if (key == "movecount") {
            config.requiredConsecutiveMoves = QaplaHelpers::to_uint32(value).value_or(0);
        }
        else if (key == "score") {
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

AdjudicationManager::DrawAdjudicationConfig AdjudicationConfig::fromDrawManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto draw = manager.getGroupInstance(groupName);
    if (!draw) {
        return AdjudicationManager::DrawAdjudicationConfig{};
    }

    return AdjudicationManager::DrawAdjudicationConfig{
        .minFullMoves = draw->get<unsigned int>("movenumber"),
        .requiredConsecutiveMoves = draw->get<unsigned int>("movecount"),
        .centipawnThreshold = draw->get<int>("score"),
        .testOnly = draw->get<bool>("test")
    };
}

AdjudicationManager::ResignAdjudicationConfig AdjudicationConfig::fromResignManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto resign = manager.getGroupInstance(groupName);
    if (!resign) {
        return AdjudicationManager::ResignAdjudicationConfig{};
    }

    return AdjudicationManager::ResignAdjudicationConfig{
        .requiredConsecutiveMoves = resign->get<unsigned int>("movecount"),
        .centipawnThreshold = resign->get<int>("score"),
        .testOnly = resign->get<bool>("test")
    };
}

} // namespace QaplaTester
