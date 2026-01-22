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
#include "../cli/settings-manager.h"
#include "../cli/settings-definitions.h"
#include <fstream>
#include <stdexcept>

namespace QaplaTester {

using Settings::ValueType;

QaplaHelpers::ConfigData SprtTournamentFile::configData_;
Settings::Manager SprtTournamentFile::manager_;

namespace {
    std::unordered_map<std::string, Settings::Definition> addIdKey(std::unordered_map<std::string, Settings::Definition> keys) {
        keys["id"] = { .description = "Identifier for the tournament configuration",
                       .isRequired = true,
                       .defaultValue = "",
                       .type = ValueType::String };
        return keys;
    }
}

Settings::Manager SprtTournamentFile::registerSettingsGroups() {
    Settings::Manager manager;
    manager.registerGroup("eachengine", "Global engine configuration", false, {
        { "id",        { .description = "Identifier for the tournament configuration",
                        .isRequired = true,
                        .defaultValue = "",
                        .type = ValueType::String } },
        { "usehash",   { .description = "Enable global hash size setting",
                        .isRequired = false,
                        .defaultValue = false,
                        .type = ValueType::Bool } },
        { "hash",      { .description = "Hash table size in MB",
                        .isRequired = false,
                        .defaultValue = 32,
                        .type = ValueType::UInt } },
        { "useponder", { .description = "Enable global ponder setting",
                        .isRequired = false,
                        .defaultValue = false,
                        .type = ValueType::Bool } },
        { "ponder",    { .description = "Enable pondering, if the engine supports it",
                        .isRequired = false,
                        .defaultValue = false,
                        .type = ValueType::Bool } },
        { "usetrace",  { .description = "Enable global trace setting",
                        .isRequired = false,
                        .defaultValue = false,
                        .type = ValueType::Bool } },
        { "trace",     { .description = "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work",
                        .isRequired = false,
                        .defaultValue = "command",
                        .type = ValueType::String } },
        { "userestart", { .description = "Enable global restart setting",
                        .isRequired = false,
                        .defaultValue = false,
                        .type = ValueType::Bool } },
        { "restart",   { .description = "Engine restart mode: auto (engine decides), on (always), or off (never)",
                        .isRequired = false,
                        .defaultValue = "auto",
                        .type = ValueType::String } }
    });
    manager.registerGroup("engineselection", "Engine selection for tournament", false, {
        { "id",           { .description = "Identifier for the tournament configuration",
                           .isRequired = true,
                           .defaultValue = "",
                           .type = ValueType::String } },
        { "name",         { .description = "Name of the engine",
                           .isRequired = false,
                           .defaultValue = "",
                           .type = ValueType::String } },
        { "originalName", { .description = "Original name of the engine before modification",
                           .isRequired = false,
                           .defaultValue = "",
                           .type = ValueType::String } },
        { "selected",     { .description = "Whether this engine is selected for the tournament",
                           .isRequired = false,
                           .defaultValue = false,
                           .type = ValueType::Bool } },
        { "author",       { .description = "Author of the engine",
                           .isRequired = false,
                           .defaultValue = "",
                           .type = ValueType::String } },
        { "cmd",          { .description = "Path to executable",
                           .isRequired = false,
                           .defaultValue = "",
                           .type = ValueType::PathExists } },
        { "dir",          { .description = "Working directory",
                           .isRequired = false,
                           .defaultValue = std::nullopt,
                           .type = ValueType::PathExists } },
        { "proto",        { .description = "Protocol (uci/xboard)",
                           .isRequired = false,
                           .defaultValue = std::nullopt,
                           .type = ValueType::String } },
        { "tc",           { .description = "Time control in format moves/time+inc or 'inf'",
                           .isRequired = false,
                           .defaultValue = std::nullopt,
                           .type = ValueType::String } },
        { "ponder",       { .description = "Enable pondering, if the engine supports it",
                           .isRequired = false,
                           .defaultValue = std::nullopt,
                           .type = ValueType::Bool } },
        { "trace",        { .description = "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work",
                           .isRequired = false,
                           .defaultValue = std::nullopt,
                           .type = ValueType::String } },
        { "restart",      { .description = "Engine restart mode: auto (engine decides), on (always), or off (never)",
                           .isRequired = false,
                           .defaultValue = std::nullopt,
                           .type = ValueType::String } },
        { "gauntlet",     { .description = "Set if engine is part of the gauntlet group.",
                           .isRequired = false,
                           .defaultValue = false,
                           .type = ValueType::Bool } },
        { "option.[name]",  { .description = "UCI engine option",
                           .isRequired = false,
                           .defaultValue = "",
                           .type = ValueType::String } }
    });
    
    manager.registerGroup("sprtconfig", "SPRT configuration", true, 
        addIdKey(Settings::getSprtKeys()));
    manager.registerGroup("opening", "Opening book configuration", true, 
        addIdKey(Settings::getOpeningsKeys()));
    manager.registerGroup("pgnoutput", "PGN output settings", true, 
        addIdKey(Settings::getPgnOutputKeys()));
    manager.registerGroup("drawadjudication", "Draw adjudication settings", true, 
        addIdKey(Settings::getDrawAdjudicationKeys()));
    manager.registerGroup("resignadjudication", "Resign adjudication settings", true, 
        addIdKey(Settings::getResignAdjudicationKeys()));
    
    manager.registerGroup("timecontroloptions", "Time control options", false, {
        { "id",          { .description = "Identifier for the tournament configuration",
                          .isRequired = true,
                          .defaultValue = "",
                          .type = ValueType::String } },
        { "timeControl", { .description = "Time control in format moves/time+inc or 'inf'",
                          .isRequired = false,
                          .defaultValue = "",
                          .type = ValueType::String } }
    });
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

    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("Failed to open file for reading: " + filename);
    }

    manager_ = registerSettingsGroups();
    configData_ = QaplaHelpers::ConfigData{};

    // Load all sections from the file into a temporary ConfigData
    QaplaHelpers::ConfigData tempConfigData;
    tempConfigData.load(in);
    in.close();

    // Transfer sections with the specified id to the target ConfigData
    for (const auto& sectionName : sectionNames) {
        auto sections = tempConfigData.getSectionList(sectionName, id);
        if (!sections || sections->empty()) {
            if (std::string(sectionName) == "round") {
                continue;
            }
            throw std::runtime_error("Missing required section '" + std::string(sectionName) + 
                                   "' in SPRT tournament file: " + filename);
        }
        configData_.setSectionList(sectionName, id, *sections);
    }
    manager_.parseInput(configData_);
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
