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
#include <fstream>
#include <stdexcept>

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

} // namespace QaplaTester
