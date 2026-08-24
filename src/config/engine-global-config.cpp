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
        {"id", id}
    };
    
    entries.emplace_back("usehash", config.useGlobalHash ? "true" : "false");
    if (config.useGlobalHash) {
        entries.emplace_back("hash", std::to_string(config.hashSizeMB));
    }
    
    entries.emplace_back("useponder", config.useGlobalPonder ? "true" : "false");
    if (config.useGlobalPonder) {
        entries.emplace_back("ponder", config.ponder ? "true" : "false");
    }
    
    entries.emplace_back("usetrace", config.useGlobalTrace ? "true" : "false");
    if (config.useGlobalTrace) {
        entries.emplace_back("trace", config.traceLevel);
    }
    
    entries.emplace_back("userestart", config.useGlobalRestart ? "true" : "false");
    if (config.useGlobalRestart) {
        entries.emplace_back("restart", config.restart);
    }

    // One switch, four values: see EngineGlobalConfig::useGlobalSyzygy.
    entries.emplace_back("usesyzygy", config.useGlobalSyzygy ? "true" : "false");
    if (config.useGlobalSyzygy) {
        entries.emplace_back("syzygypath", config.syzygyPath);
        entries.emplace_back("syzygyprobedepth", std::to_string(config.syzygyProbeDepth));
        entries.emplace_back("syzygyprobelimit", std::to_string(config.syzygyProbeLimit));
        entries.emplace_back("syzygy50moverule", config.syzygy50MoveRule ? "true" : "false");
    }
    
    return {
        .name = getSectionName(),
        .entries = entries
    };
}

EngineGlobalConfig EngineGlobalConfigFile::fromSection(
    const QaplaHelpers::IniFile::Section& section) {
    
    EngineGlobalConfig config;
    
    for (const auto& [key, value] : section.entries) {
        if (key == "usehash") {
            config.useGlobalHash = (value == "true" || value == "1");
        }
        else if (key == "hash") {
            config.hashSizeMB = QaplaHelpers::to_uint32(value).value_or(32);
        }
        else if (key == "useponder") {
            config.useGlobalPonder = (value == "true" || value == "1");
        }
        else if (key == "ponder") {
            config.ponder = (value == "true" || value == "1");
        }
        else if (key == "usetrace") {
            config.useGlobalTrace = (value == "true" || value == "1");
        }
        else if (key == "trace") {
            config.traceLevel = value;
        }
        else if (key == "userestart") {
            config.useGlobalRestart = (value == "true" || value == "1");
        }
        else if (key == "restart") {
            config.useGlobalRestart = true;
            config.restart = value;
        }
        else if (key == "usesyzygy") {
            config.useGlobalSyzygy = (value == "true" || value == "1");
        }
        else if (key == "syzygypath") {
            config.syzygyPath = value;
        }
        else if (key == "syzygyprobedepth") {
            config.syzygyProbeDepth = QaplaHelpers::to_uint32(value).value_or(config.syzygyProbeDepth);
        }
        else if (key == "syzygyprobelimit") {
            config.syzygyProbeLimit = QaplaHelpers::to_uint32(value).value_or(config.syzygyProbeLimit);
        }
        else if (key == "syzygy50moverule") {
            config.syzygy50MoveRule = (value == "true" || value == "1");
        }
    }
    
    return config;
}

std::vector<QaplaHelpers::IniFile::Section> EngineGlobalConfigFile::toEngineConfigSections(
    const EngineGlobalConfig& config, 
    const std::string& id) {
    
    return { toSection(config, id) };
}

std::vector<QaplaHelpers::IniFile::Section> EngineGlobalConfigFile::toTimeControlSections(
    const EngineGlobalConfig& config, 
    const std::string& id) {
    
    return { createTimeControlSection(config, id) };
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
    
    auto sections = configData.getSectionList(getSectionName(), id);
    if (!sections || sections->empty()) {
        return std::nullopt;
    }
    
    auto config = fromSections(*sections);
    
    auto timeControl = loadTimeControl(configData, id);
    if (timeControl) {
        config.timeControl = *timeControl;
    }
    
    return config;
}

void EngineGlobalConfigFile::applyGlobalConfig(
    EngineConfig& engine,
    const EngineGlobalConfig& globalConfig) {
    
    if (globalConfig.useGlobalPonder) {
        engine.setPonder(globalConfig.ponder);
    }
    
    if (globalConfig.useGlobalRestart) {
        engine.setRestartOption(parseRestartOption(globalConfig.restart));
    }
    
    if (globalConfig.useGlobalTrace) {
        engine.setTraceLevel(globalConfig.traceLevel);
    }
    
    if (globalConfig.useGlobalHash) {
        engine.setOptionValue("Hash", std::to_string(globalConfig.hashSizeMB));
    }

    // All four, or none: they are one setting split over four engine options. An engine that does
    // not offer one of them ignores it -- UciAdapter::setOptionValues() sends only what the engine
    // reported. An empty path is passed on as an empty path, which is how an engine is told to use
    // no tablebases; switching the setting off instead is what leaves each engine's own alone.
    if (globalConfig.useGlobalSyzygy) {
        engine.setOptionValue("SyzygyPath", globalConfig.syzygyPath);
        engine.setOptionValue("SyzygyProbeDepth", std::to_string(globalConfig.syzygyProbeDepth));
        engine.setOptionValue("SyzygyProbeLimit", std::to_string(globalConfig.syzygyProbeLimit));
        engine.setOptionValue("Syzygy50MoveRule", globalConfig.syzygy50MoveRule ? "true" : "false");
    }
    
    engine.setTimeControl(globalConfig.timeControl);
}

std::optional<std::string> EngineGlobalConfigFile::loadTimeControl(
    const QaplaHelpers::ConfigData& configData,
    const std::string& id) {
    
    auto sections = configData.getSectionList("timecontroloptions", id);
    if (sections && !sections->empty()) {
        return sections->front().getValue("timeControl");
    }
    return std::nullopt;
}

QaplaHelpers::IniFile::Section EngineGlobalConfigFile::createTimeControlSection(
    const EngineGlobalConfig& config,
    const std::string& id) {
    
    QaplaHelpers::IniFile::KeyValueMap entries{
        {"id", id},
        {"timeControl", config.timeControl}
    };
    
    return {
        .name = "timecontroloptions",
        .entries = entries
    };
}

} // namespace QaplaTester
