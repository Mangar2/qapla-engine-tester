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

#pragma once

#include <string>
#include <stdexcept>
#include <vector>
#include <optional>
#include <utility>
#include <unordered_map>
#include <map>

#include "string-helper.h"

namespace QaplaHelpers {

class IniFile {
public:
    using KeyValueMap = std::vector<std::pair<std::string, std::string>>;
    struct Section {
        std::string name;
        KeyValueMap entries;
        /**
         * @brief Adds an entry to the section.
         * @param key The key of the entry.
         * @param value The value of the entry.
         */
        void addEntry(const std::string& key, const std::string& value) {
            entries.emplace_back(key, value);
        }   
        /**
         * @brief Inserts an entry at the beginning of the section.
         * @param key The key of the entry.
         * @param value The value of the entry.
         */
        void insertFirst(const std::string& key, const std::string& value) {
            entries.insert(entries.begin(), { key, value });
        }

        /**
         * @brief Deletes all entries with the specified key.
         * @param key The key of the entries to delete.
         */
        void eraseEntry(const std::string& key) {
            std::erase_if(entries, [&](const auto& pair) {
                return pair.first == key;
            });
        }

        /**
         * @brief Retrieves the value for a given key.
         * @param key The key to look up.
         * @return An optional containing the value if found, or std::nullopt if not found.
         */
        [[nodiscard]] std::optional<std::string> getValue(const std::string& key) const {
            for (const auto& [k, v] : entries) {
                if (k == key) {
                    return v;
                }
            }
            return std::nullopt;
        }

        /**
         * @brief Returns the entries as an unordered map.
         * @return Unordered map of key-value pairs.
         */
        [[nodiscard]] std::unordered_map<std::string, std::string> getUnorderedMap() const {
            std::unordered_map<std::string, std::string> map;
            for (const auto& [k, v] : entries) {
                map[k] = v;
            }
            return map;
        }
    };

    using SectionList = std::vector<Section>;

    /**
     * @brief Renames all sections in a SectionList.
     * @param sections The list of sections to modify.
     * @param oldName Current section name to match.
     * @param newName New section name to assign.
     * @return A new SectionList with renamed sections.
     */
    static SectionList renameSections(const SectionList& sections, 
                                      const std::string& oldName, 
                                      const std::string& newName) {
        SectionList result;
        result.reserve(sections.size());
        for (const auto& section : sections) {
            Section newSection = section;
            if (section.name == oldName) {
                newSection.name = newName;
            }
            result.push_back(std::move(newSection));
        }
        return result;
    }

    /**
     * @brief Loads the INI file sections from the input stream.
     * @warning This function is deprecated. Use ConfigData class for more advanced configuration handling.
     *
     * @param in The input stream to read from.
     * @return SectionList A list of sections parsed from the INI file.
     */
    static SectionList load(std::istream& in);    

    /**
     * @brief Saves an INI file section to the output stream.
     *
     * @param out The output stream to write to.
     * @param section The section to save.
     */
    static void saveSection(std::ostream& out, const Section& section);

    /**
     * @brief Saves multiple INI file sections to the output stream.
     *
     * @param out The output stream to write to.
     * @param sections The list of sections to save.
     */
    static void saveSections(std::ostream& out, const SectionList& sections);
};

class ConfigData {
public:
    using SectionMap = std::map<std::string, IniFile::SectionList>; 
    ~ConfigData() = default;

    /**
     * @brief Creates ConfigData from command line arguments.
     * Converts argv format (--section key=value) to ConfigData structure.
     * @param args Vector of command line arguments.
     * @return ConfigData instance populated from arguments.
     */
    static ConfigData fromArgv(const std::vector<std::string>& args);

    /**
     * @brief Saves the configuration data to the output stream in INI file format.
     * @param out The output stream to write the configuration data to.
     */
    void save(std::ostream& out);

    /**
     * @brief Loads the configuration data from an INI file stream.
     * Top-level key-value pairs before the first [section] are stored as global parameters.
     * @param in The input stream to load the configuration data from.
     */
    void load(std::istream& in);

    /**
     * @brief Loads the configuration data from an INI file.
     * Top-level key-value pairs before the first [section] are stored as global parameters.
     * @param filename The path to the INI file to load.
     * @throws std::runtime_error if the file cannot be opened.
     */
    void load(const std::string& filename);

    /**
     * @brief Adds a section to the configuration data.
     * If a section with the same name and id already exists, it will be appended to the list.
     * @param section The section to add.
     */
    void addSection(const IniFile::Section& section);

    /**
     * @brief Sets a specific section in the configuration data.
     * If a section with the same name and id already exists, it will be replaced.
     * @param section The section to set.
     */
    void setSectionList(const std::string& name, const std::string& id, 
        const IniFile::SectionList& sectionList);

    /**
     * @brief Retrieves all sections with the given name.
     * @param name The name of the sections to retrieve.
     * @return An optional containing a map of section ids to their corresponding section lists,
     *         or std::nullopt if no sections with the given name exist.
     */
    [[nodiscard]] std::optional<SectionMap> getSectionMap(const std::string& name) const;

    /**
     * @brief Retrieves a specific section from the configuration data.
     * @param name The name of the section to retrieve.
     * @param id The identifier of the section to retrieve (default is "default").
     * @return An optional containing the section if found, or std::nullopt if not found.
     */
    [[nodiscard]] std::optional<IniFile::SectionList> getSectionList(const std::string& name, const std::string& id = "default") const;

    /**
     * @brief Gets all unique section names in the configuration data.
     * @return Vector of section names.
     */
    [[nodiscard]] std::vector<std::string> getAllSectionNames() const;

    /**
     * @brief Checks if the configuration data has been modified since the last load or save operation.
     * @return true if the configuration data has been modified, false otherwise.
     */
    [[nodiscard]] bool getDirty() const {
        return dirty_;
    }

    /**
     * @brief Sets the dirty flag indicating whether the configuration data has been modified.
     * @param dirty The new value for the dirty flag.
     */
    void setDirty(bool dirty) {
        dirty_ = dirty;
    }

    /**
     * @brief Gets the global parameters (top-level key-value pairs not in any section).
     * @return Vector of key-value pairs representing global parameters.
     */
    [[nodiscard]] const IniFile::KeyValueMap& getGlobalParameters() const {
        return globalParameters_;
    }

    /**
     * @brief Adds a global parameter (top-level key-value pair).
     * @param key The parameter name.
     * @param value The parameter value.
     */
    void addGlobalParameter(const std::string& key, const std::string& value) {
        globalParameters_.emplace_back(key, value);
        dirty_ = true;
    }

private:
    bool dirty_ = false;
    using SectionTree = std::unordered_map<std::string, SectionMap>;
    SectionTree sectionTree_;
    IniFile::KeyValueMap globalParameters_;
};

} // namespace QaplaHelpers
