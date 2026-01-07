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

#include "ini-file.h"
#include "string-helper.h"

namespace QaplaHelpers {

    IniFile::SectionList IniFile::load(std::istream& in) {
        SectionList sections;
        std::optional<Section> currentSection;

        std::string line;
        while (std::getline(in, line)) {
            line = QaplaHelpers::trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }

            if (line.front() == '[' && line.back() == ']') {
                if (currentSection) {
                    sections.push_back(std::move(*currentSection));
                }
                currentSection = Section{ 
                    .name=QaplaHelpers::trim(line.substr(1, line.size() - 2)), 
                    .entries={} 
                };
            } else {
                auto keyValue = QaplaHelpers::parseKeyValue(line);
                if (keyValue && currentSection) {
                    currentSection->addEntry(keyValue->first, keyValue->second);
                }
            }
        }
        if (currentSection) {
            sections.push_back(std::move(*currentSection));
        }
        return sections;
    }

    void IniFile::saveSection(std::ostream& out, const Section& section) {
        out << "[" << section.name << "]\n";
        for (const auto& [key, value] : section.entries) {
            out << key << "=" << value << "\n";
        }
        out << "\n";
    }

    void IniFile::saveSections(std::ostream& out, const SectionList& sections) {
        for (const auto& section : sections) {
            saveSection(out, section);
        }
    }

    void ConfigData::save(std::ostream& out) {
        for (const auto& [id, idSectionLists] : sectionTree_) {
            for (const auto& [sectionId, sectionList] : idSectionLists) {
                IniFile::saveSections(out, sectionList);
            }
        }
    }

    void ConfigData::load(std::istream& in) {
        sectionTree_.clear();
        auto iniSections = IniFile::load(in);
        for (const auto& section : iniSections) {
            addSection(section);
        }
    }

    void ConfigData::addSection(const IniFile::Section& section) {
        auto name = section.name;
        auto idOpt = section.getValue("id");
        auto id = idOpt ? *idOpt : "default";
        if (!sectionTree_[name].contains(id)) {
            sectionTree_[name][id] = IniFile::SectionList{};
        }
        sectionTree_[name][id].push_back(section);
    }

    void ConfigData::setSectionList(const std::string& name, const std::string& id, 
        const IniFile::SectionList& sectionList)
    {
        setDirty(true);
        if (!id.empty()) {
            sectionTree_[name][id] = sectionList;
        } else {
            sectionTree_[name]["default"] = sectionList;
        }
    }

    std::optional<ConfigData::SectionMap> ConfigData::getSectionMap(const std::string& name) const {
        auto it = sectionTree_.find(name);
        if (it != sectionTree_.end()) {
            return it->second;
        }
        return std::nullopt;    
    }

    std::optional<IniFile::SectionList> ConfigData::getSectionList(const std::string& name, const std::string& id) const {
        auto sectionMapOpt = getSectionMap(name);
        if (!sectionMapOpt) {
            return std::nullopt;
        }
        auto it = sectionMapOpt->find(id);
        if (it != sectionMapOpt->end()) {
            return it->second;
        }
        return std::nullopt;
    }

} // namespace QaplaHelpers