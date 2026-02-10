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
#include <fstream>

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

    ConfigData ConfigData::fromArgv(const std::vector<std::string>& args) {
        ConfigData configData;
        IniFile::Section currentSection{.name = "", .entries = {}};
        bool inSection = false;

        for (size_t idx = 1; idx < args.size(); ++idx) {
            const std::string& arg = args[idx];
            std::string trimmed = QaplaHelpers::trim(arg);
            
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            if (trimmed.starts_with("--")) {
                auto rest = trimmed.substr(2);
                auto eqPos = rest.find('=');
                
                if (eqPos == std::string::npos) {
                    // Check if next arg is a grouped parameter (no -- prefix)
                    bool isGroupStart = false;
                    if (idx + 1 < args.size()) {
                        std::string nextArg = QaplaHelpers::trim(args[idx + 1]);
                        if (!nextArg.empty() && !nextArg.starts_with("--") && nextArg.find('=') != std::string::npos) {
                            isGroupStart = true;
                        }
                    }
                    
                    if (isGroupStart) {
                        if (inSection) {
                            configData.addSection(currentSection);
                        }
                        currentSection = IniFile::Section{.name = rest, .entries = {}};
                        inSection = true;
                    } else {
                        // Treat as boolean flag
                        if (inSection) {
                            currentSection.addEntry(rest, "true");
                        } else {
                            configData.addGlobalParameter(rest, "true");
                        }
                    }
                } else {
                    std::string key = rest.substr(0, eqPos);
                    std::string value = rest.substr(eqPos + 1);
                    if (inSection) {
                        currentSection.addEntry(key, value);
                    } else {
                        configData.addGlobalParameter(key, value);
                    }
                }
            } else {
                auto eqPos = trimmed.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = trimmed.substr(0, eqPos);
                    std::string value = trimmed.substr(eqPos + 1);
                    currentSection.addEntry(key, value);
                } else {
                    currentSection.addEntry(trimmed, "true");
                }
            }
        }

        if (inSection) {
            configData.addSection(currentSection);
        }

        return configData;
    }

    void ConfigData::save(std::ostream& out) {
        for (const auto& [id, idSectionLists] : sectionTree_) {
            for (const auto& [sectionId, sectionList] : idSectionLists) {
                IniFile::saveSections(out, sectionList);
            }
        }
    }

    void ConfigData::load(const std::string& filename) {
        std::ifstream in(filename);
        if (!in) {
            throw std::runtime_error("Failed to open file for reading: " + filename);
        }
        load(in);
    }

    void ConfigData::load(std::istream& in) {
        sectionTree_.clear();
        globalParameters_.clear();
        std::string line;
        bool inSection = false;
        std::string content;

        while (std::getline(in, line)) {
            std::string trimmed = QaplaHelpers::trim(line);
            if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
                content += line + "\n";
                continue;
            }
            if (trimmed.front() == '[' && trimmed.back() == ']') {
                inSection = true;
            } else if (!inSection) {
                auto keyValue = QaplaHelpers::parseKeyValue(trimmed);
                if (keyValue) {
                    addGlobalParameter(keyValue->first, keyValue->second);
                    continue;
                }
            }
            content += line + "\n";
        }

        std::istringstream contentStream(content);
        auto iniSections = IniFile::load(contentStream);
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

    std::vector<std::string> ConfigData::getAllSectionNames() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : sectionTree_) {
            names.push_back(name);
        }
        return names;
    }

    void ConfigData::setKeyInAllSections(const std::string& key, const std::string& value) {
        for (auto& [name, sectionMap] : sectionTree_) {
            for (auto& [id, sectionList] : sectionMap) {
                for (auto& section : sectionList) {
                    section.changeOrAddEntry(key, value);
                }
            }
        }
        setDirty(true);
    }

} // namespace QaplaHelpers