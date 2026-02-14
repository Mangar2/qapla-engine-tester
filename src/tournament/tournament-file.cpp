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

#include "tournament-file.h"
#include "tournament.h"
#include "../cli/settings-manager.h"
#include "../base-elements/logger.h"
#include <fstream>
#include <stdexcept>
#include <format>
#include <filesystem>

namespace QaplaTester {

void TournamentFile::save(const std::string& filename, 
                          const QaplaHelpers::ConfigData& configData,
                          const std::string& id) {
    
    if (filename.empty()) {
        throw std::invalid_argument("No filename specified for saving tournament.");
    }

    std::ofstream out(filename, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Save each section type with the specified id
    for (const auto& sectionName : sectionNames) {
        auto sections = configData.getSectionList(sectionName, id);
        if (!sections || sections->empty()) {
            sections = configData.getSectionList(sectionName, "all");
        }
        if (sections && !sections->empty()) {
            // All entries shall have a turnament id, even if they are from the "all" section. 
            auto tourSections = QaplaHelpers::IniFile::setValueInSections(*sections, "id", id);
            for (auto& section : tourSections) {
                auto name = section.name;
                // Avoid recursive self-reference: The 'file' entry contains the name of this state file.
                if (name == "tournament") {
                    section.eraseEntry("file");
                }
                QaplaHelpers::IniFile::saveSection(out, section);
            }
        }
    }

    out.close();
    if (!out) {
        throw std::runtime_error("Error while writing to file: " + filename);
    }
}

void TournamentFile::save(const std::string& filename, 
                          const Settings::Manager& settingsManager,
                          const std::shared_ptr<Tournament>& tournament,
                          const std::string& id) {
    if (!filename.empty()) {
        auto sections = tournament->getSections();
        QaplaHelpers::ConfigData configData = settingsManager.toConfigData({});
        for (const auto& section : sections) {
            configData.addSection(section);
        }
        save(filename, configData, id);
    }
}

void TournamentFile::setSaveCallback(const std::string& filename, uint32_t saveInterval, 
                                     const std::shared_ptr<Tournament>& tournament, 
                                     const std::vector<EngineConfig>& engines) {
    // Setup autosave callback if file is specified
    if (!filename.empty()) {
        tournament->setSaveCallback(
            [filename,
             configData = Settings::Manager::instance().toConfigData({}),
             engines,
             tournament = tournament.get()]() mutable 
            {
                auto sections = tournament->getSections();
                auto saveData = configData;

                for (const auto& engine : engines) {
                    auto engineSection = engine.toSection();
                    engineSection.addEntry("id", TournamentFile::id);
                    saveData.addSection(engineSection);
                }

                for (const auto& section : sections) {
                    saveData.addSection(section);
                }
                TournamentFile::save(filename, saveData, TournamentFile::id);
                Logger::reportLogger().log(std::format("Auto-saved tournament state to: {}", filename), TraceLevel::info);
            }, saveInterval
        );
    }
}

void TournamentFile::loadGameResults(const std::string& filename, 
                                     const std::shared_ptr<Tournament>& tournament, 
                                     const std::string& id) {
    if (filename.empty() || !std::filesystem::exists(filename)) {
        return;
    }
    QaplaHelpers::ConfigData configData;
    configData.load(filename);
    auto sections = configData.getSectionList("round", id);
    if (sections) {
        tournament->load(*sections);
    }
}

} // namespace QaplaTester
