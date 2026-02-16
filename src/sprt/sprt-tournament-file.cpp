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
#include "../base-elements/logger.h"
#include "src/base-elements/ini-file.h"
#include <stdexcept>
#include <string_view>

namespace QaplaTester {

QaplaHelpers::ConfigData SprtTournamentFile::getConfigData(const Settings::Manager& settingsManager) {
    std::vector<std::string> sectionFilter;
    sectionFilter.reserve(sectionNames.size());

    for (const auto* sectionName : sectionNames) {
        const auto sectionNameView = std::string_view(sectionName);
        if ((sectionNameView != "engine") && (sectionNameView != "round")) {
            sectionFilter.emplace_back(sectionName);
        }
    }

    return settingsManager.toConfigData(sectionFilter);
}

 void SprtTournamentFile::setSaveCallback(const std::string& filename, uint32_t saveIntervalMs, 
    const std::shared_ptr<SprtManager>& manager,
    const std::vector<EngineConfig>& engines) {
    
    // Setup autosave callback if file is specified
    if (!filename.empty()) {
        
        manager->setSaveCallback(
            [filename,
             configData = getConfigData(Settings::Manager::instance()),
             manager = manager.get(),
             engines
            ]() mutable 
            {
                auto saveData = configData;
                auto section = manager->getSection();
                if (section) {
                    saveData.addSection(*section);
                }

                for (const auto& engine : engines) {
                    auto engineSection = engine.toSection();
                    engineSection.addEntry("id", id);
                    saveData.addSection(engineSection);
                }

                SprtTournamentFile::save(filename, saveData);
                Logger::reportLogger().log(std::format("Auto-saved SPRT state to: {}", filename), TraceLevel::info);
            }, saveIntervalMs);
    }
}

void SprtTournamentFile::save(const std::string& filename,
    const QaplaHelpers::ConfigData& configData,
    const std::string& id) 
{
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
        if (!sections || sections->empty()) {
            sections = configData.getSectionList(sectionName, "all");
        }
        if (sections && !sections->empty()) {
            // All entries shall have a sprt-tournament id, even if they are from the "all" section. 
            auto sprtSections = QaplaHelpers::IniFile::setValueInSections(*sections, "id", id);
            for (auto& section : sprtSections) {
                auto name = section.name;
                // Avoid recursive self-reference: The 'file' entry contains the name of this state file.
                if (name == "sprt") {
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

void SprtTournamentFile::save(
    const std::string& filename, 
    const Settings::Manager& settingsManager,
    const std::shared_ptr<SprtManager>& sprtManager,
    const std::string& id) 
{
    if (!filename.empty()) {
        auto section = sprtManager->getSection();
        auto configData = getConfigData(settingsManager);
        if (section) {
            configData.addSection(*section);
        }
        save(filename, configData, id);
    }
}

void SprtTournamentFile::loadGameResults(
    const std::string& filename, 
    const std::shared_ptr<SprtManager>& manager,
    const std::string& id) 
{
    if (filename.empty() || !std::filesystem::exists(filename)) {
        return;
    }
    QaplaHelpers::ConfigData configData;
    configData.load(filename);
    auto sections = configData.getSectionList("round", id);
    if (sections) {
        manager->setGameResults(*sections);
    }
}


} // namespace QaplaTester
