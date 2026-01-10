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

#include "engine-global-config.h"
#include "../base-elements/string-helper.h"

namespace QaplaTester {

QaplaHelpers::IniFile::Section EngineGlobalConfigFile::toSection(
    const EngineGlobalConfig& config,
    const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"timeControl", config.timeControl}
    };
    
    if (config.useGlobalHash) {
        entries.emplace_back("Hash", std::to_string(config.hashSizeMB));
    }
    
    if (config.useGlobalPonder) {
        entries.emplace_back("ponder", config.ponder ? "true" : "false");
    }
    
    if (config.useGlobalTrace) {
        entries.emplace_back("trace", config.traceLevel);
    }
    
    if (config.useGlobalRestart) {
        entries.emplace_back("restart", config.restart);
    }
    
    return {
        .name = "eachengine",
        .entries = entries
    };
}

EngineGlobalConfig EngineGlobalConfigFile::fromSection(
    const QaplaHelpers::IniFile::Section& section) {
    
    EngineGlobalConfig config;
    
    for (const auto& [key, value] : section.entries) {
        if (key == "timeControl" || key == "tc") {
            config.timeControl = value;
        }
        else if (key == "Hash") {
            config.useGlobalHash = true;
            config.hashSizeMB = QaplaHelpers::to_uint32(value).value_or(32);
        }
        else if (key == "ponder") {
            config.useGlobalPonder = true;
            config.ponder = (value == "true" || value == "1");
        }
        else if (key == "trace") {
            config.useGlobalTrace = true;
            config.traceLevel = value;
        }
        else if (key == "restart") {
            config.useGlobalRestart = true;
            config.restart = value;
        }
    }
    
    return config;
}

std::vector<QaplaHelpers::IniFile::Section> EngineGlobalConfigFile::getSections(
    const EngineGlobalConfig& config, 
    const std::string& id) {
    
    return { toSection(config, id) };
}

EngineGlobalConfig EngineGlobalConfigFile::fromSections(
    const std::vector<QaplaHelpers::IniFile::Section>& sections) {
    
    EngineGlobalConfig config;
    
    if (sections.empty()) {
        return config;
    }
    
    return fromSection(sections[0]);
}

std::optional<EngineGlobalConfig> EngineGlobalConfigFile::fromConfigData(
    const QaplaHelpers::ConfigData& configData, 
    const std::string& id) {
    
    auto sections = configData.getSectionList("eachengine", id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }
    
    return fromSections(*sections);
}

void EngineGlobalConfigFile::applyGlobalConfig(
    EngineConfig& engine,
    const EngineGlobalConfig& globalConfig) {
    
    if (globalConfig.useGlobalPonder) {
        engine.setPonder(globalConfig.ponder);
    }
    
    engine.setTimeControl(globalConfig.timeControl);
    
    if (globalConfig.useGlobalRestart) {
        engine.setRestartOption(parseRestartOption(globalConfig.restart));
    }
    
    if (globalConfig.useGlobalTrace) {
        engine.setTraceLevel(globalConfig.traceLevel);
    }
    
    if (globalConfig.useGlobalHash) {
        engine.setOptionValue("Hash", std::to_string(globalConfig.hashSizeMB));
    }
}

} // namespace QaplaTester
