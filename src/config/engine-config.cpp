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

#include "engine-config.h"

namespace QaplaTester {

QaplaHelpers::IniFile::Section EngineConfigFile::toSection(
    const EngineConfiguration& config, const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries {
        {"id", id},
        {"name", config.config.getName()},
        {"originalName", config.originalName},
        {"selected", config.selected ? "true" : "false"},
        {"cmd", config.config.getCmd()},
        {"dir", config.config.getDir()},
        {"proto", to_string(config.config.getProtocol())},
        {"trace", to_string(config.config.getTraceLevel())},
        {"restart", to_string(config.config.getRestartOption())}
    };
    
    if (!config.config.getAuthor().empty()) {
        entries.emplace_back("author", config.config.getAuthor());
    }
    
    if (!config.config.getArgs().empty()) {
        entries.emplace_back("args", config.config.getArgs());
    }
    
    const auto timeControl = config.config.getTimeControl().toPgnTimeControlString();
    if (!timeControl.empty()) {
        entries.emplace_back("tc", timeControl);
    }
    
    if (config.config.isPonderEnabled()) {
        entries.emplace_back("ponder", "true");
    }
    
    if (config.config.isScoreFromWhitePov()) {
        entries.emplace_back("whitepov", "true");
    }
    
    if (config.config.isGauntlet()) {
        entries.emplace_back("gauntlet", "true");
    }
    
    for (const auto& [name, value] : config.config.getOptionValues()) {
        entries.emplace_back(name, value);
    }
    
    return {.name = getSectionName(), .entries = entries};
}

EngineConfiguration EngineConfigFile::fromSection(const QaplaHelpers::IniFile::Section& section) {
    EngineConfiguration result;
    
    for (const auto& [key, value] : section.entries) {
        result.config.setValue(key, value);
    }
    
    result.originalName = section.getValue("originalName").value_or(result.config.getName());
    result.selected = section.getValue("selected").value_or("false") == "true";
    
    return result;
}

std::vector<QaplaHelpers::IniFile::Section> EngineConfigFile::toSections(
    const std::vector<EngineConfiguration>& configs, const std::string& id) {
    
    std::vector<QaplaHelpers::IniFile::Section> sections;
    sections.reserve(configs.size());
    
    for (const auto& config : configs) {
        sections.push_back(toSection(config, id));
    }
    
    return sections;
}

std::vector<EngineConfiguration> EngineConfigFile::fromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& sections) {
    
    std::vector<EngineConfiguration> configs;
    configs.reserve(sections.size());
    
    for (const auto& section : sections) {
        if (section.name == getSectionName()) {
            configs.push_back(fromSection(section));
        }
    }
    
    return configs;
}

std::optional<std::vector<EngineConfiguration>> EngineConfigFile::fromConfigData(
    const QaplaHelpers::ConfigData& configData, 
    const std::string& id) {
    
    auto sections = configData.getSectionList(getSectionName(), id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }
    
    return fromSections(*sections);
}

} // namespace QaplaTester
