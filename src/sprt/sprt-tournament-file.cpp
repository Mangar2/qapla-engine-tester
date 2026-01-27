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

#include "sprt-tournament-file.h"
#include "sprt-manager.h"
#include "sprt-config-file.h"
#include "../config-file/opening-config.h"
#include "../config-file/pgn-config.h"
#include "../config-file/adjudication-config.h"
#include "../cli/settings-manager.h"
#include "../cli/settings-definitions.h"
#include <stdexcept>

namespace QaplaTester {

using Settings::ValueType;

QaplaHelpers::ConfigData SprtTournamentFile::configData_;
Settings::Manager SprtTournamentFile::manager_;

Settings::Manager SprtTournamentFile::registerSettingsGroups() {
    Settings::Manager manager;
    manager.registerGroup({.name = "each", .description = "Global engine configuration", .unique = false, 
        .keys = Settings::getEachKeys()});

    manager.registerGroup({.name = "engine", .description = "Engine selection for tournament", .unique = false, 
        .keys = Settings::getEngineKeys()});
    
    manager.registerGroup({.name = SprtConfigFile::getSectionName(), .description = "SPRT configuration", .unique = true, 
        .keys = Settings::getSprtKeys()});
    manager.registerGroup({.name = OpeningConfig::getSectionName(), .description = "Opening book configuration", .unique = true, 
        .keys = Settings::getOpeningsKeys()});
    manager.registerGroup({.name = PgnConfig::getSectionName(), .description = "PGN output settings", .unique = true, 
        .keys = Settings::getPgnOutputKeys()});
    manager.registerGroup({.name = AdjudicationConfig::getDrawSectionName(), .description = "Draw adjudication settings", .unique = true, 
        .keys = Settings::getDrawAdjudicationKeys()});
    manager.registerGroup({.name = AdjudicationConfig::getResignSectionName(), .description = "Resign adjudication settings", .unique = true, 
        .keys = Settings::getResignAdjudicationKeys()});    
    
    return manager;
}

void SprtTournamentFile::save(const std::string& filename,
                               const QaplaHelpers::ConfigData& configData,
                               const std::string& id) {
    if (filename.empty()) {
        throw std::invalid_argument("No filename specified for saving SPRT tournament.");
    }

    std::ofstream out(filename, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Save each section type with the specified id
    for (const auto& sectionName : sectionNames) {
        auto sections = configData.getSectionList(sectionName, id);
        if (sections && !sections->empty()) {
            for (const auto& section : *sections) {
                QaplaHelpers::IniFile::saveSection(out, section);
            }
        }
    }

    out.close();
    if (!out) {
        throw std::runtime_error("Error while writing to file: " + filename);
    }
}

void SprtTournamentFile::load(const std::string& filename, 
                               const std::string& id) {
    
    if (filename.empty()) {
        throw std::invalid_argument("No filename specified for loading SPRT tournament.");
    }

    manager_ = registerSettingsGroups();
    configData_ = QaplaHelpers::ConfigData{};

    configData_.load(filename);

    for (const auto& sectionName : sectionNames) {
        auto sections = configData_.getSectionList(sectionName, id);
        if (!sections || sections->empty()) {
            if (std::string(sectionName) == "round") {
                continue;
            }
            throw std::runtime_error("Missing required section '" + std::string(sectionName) + 
                                   "' in SPRT tournament file: " + filename);
        }
    }
    manager_.parseInput(configData_, false);
}

bool SprtTournamentFile::loadSprtSettings(const std::string& filename,
                                        const std::string& id) {
    if (filename.empty()) {
        return false;
    }

    try {
        load(filename, id);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

QaplaHelpers::ConfigData& SprtTournamentFile::getConfigData() {
    return configData_;
}

Settings::Manager& SprtTournamentFile::getManager() {
    return manager_;
}

bool SprtTournamentFile::loadIntoManagerFromConfigData(const QaplaHelpers::ConfigData& configData,
                                                      SprtManager& manager,
                                                      const std::string& id) {
    try {
        auto sections = configData.getSectionList("round", id);
        if (sections && !sections->empty()) {
            manager.setGameResults(*sections);
            return true;
        }
    } catch (const std::exception&) {
        return false;
    }
    
    return false;
}

} // namespace QaplaTester
