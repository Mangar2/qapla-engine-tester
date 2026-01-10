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

#include "engine-config-file.h"
#include "../base-elements/string-helper.h"

namespace QaplaTester {

QaplaHelpers::IniFile::Section EngineConfigFile::toSection(const EngineConfig& config) {
    QaplaHelpers::IniFile::KeyValueMap entries;
    
    entries.emplace_back("name", config.getName());
    
    if (!config.getAuthor().empty()) {
        entries.emplace_back("author", config.getAuthor());
    }
    
    entries.emplace_back("cmd", config.getCmd());
    entries.emplace_back("dir", config.getDir());
    
    if (!config.getArgs().empty()) {
        entries.emplace_back("args", config.getArgs());
    }
    
    entries.emplace_back("proto", to_string(config.getProtocol()));
    entries.emplace_back("trace", to_string(config.getTraceLevel()));
    entries.emplace_back("restart", to_string(config.getRestartOption()));
    
    const auto timeControl = config.getTimeControl().toPgnTimeControlString();
    if (!timeControl.empty()) {
        entries.emplace_back("tc", timeControl);
    }
    
    if (config.isPonderEnabled()) {
        entries.emplace_back("ponder", "true");
    }
    
    if (config.isScoreFromWhitePov()) {
        entries.emplace_back("whitepov", "true");
    }
    
    if (config.isGauntlet()) {
        entries.emplace_back("gauntlet", "true");
    }
    
    for (const auto& [name, value] : config.getOptionValues()) {
        entries.emplace_back(name, value);
    }
    
    return {
        .name = "engine",
        .entries = entries
    };
}

EngineConfig EngineConfigFile::fromSection(const QaplaHelpers::IniFile::Section& section) {
    EngineConfig config;
    
    for (const auto& [key, value] : section.entries) {
        config.setValue(key, value);
    }
    
    return config;
}

std::vector<QaplaHelpers::IniFile::Section> EngineConfigFile::getSections(
    const std::vector<EngineConfig>& configs) {
    
    std::vector<QaplaHelpers::IniFile::Section> sections;
    sections.reserve(configs.size());
    
    for (const auto& config : configs) {
        sections.push_back(toSection(config));
    }
    
    return sections;
}

std::vector<EngineConfig> EngineConfigFile::fromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& sections) {
    
    std::vector<EngineConfig> configs;
    configs.reserve(sections.size());
    
    for (const auto& section : sections) {
        if (section.name == "engine") {
            configs.push_back(fromSection(section));
        }
    }
    
    return configs;
}

std::optional<std::vector<EngineConfig>> EngineConfigFile::fromConfigData(
    const QaplaHelpers::ConfigData& configData, 
    const std::string& id) {
    
    auto sections = configData.getSectionList("engine", id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }
    
    return fromSections(*sections);
}

} // namespace QaplaTester
