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

#include "engine-config-manager.h"
#include "cli-settings-manager.h"
#include "string-helper.h"

namespace QaplaTester {

void EngineConfigManager::loadFromStream(std::istream& input) {

    errors.clear();

    while (input) {
        EngineConfig config;

        input >> config;
        addOrReplaceConfig(config);
    }
}

void EngineConfigManager::saveToStream(std::ostream& out) const {
    for (const auto& config : configs) {
        out << config;
        out << "\n";
    }
}

void EngineConfigManager::saveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to write file");
    }

    saveToStream(file);
}

const EngineConfig* EngineConfigManager::getConfig(const std::string& name) const {
    for (const auto& config : configs) {
        if (QaplaHelpers::to_lowercase(config.getName()) == QaplaHelpers::to_lowercase(name)) {
            return &config;
        }
    }
    return nullptr;
}

EngineConfig* EngineConfigManager::getConfigMutable(const std::string& name)  {
    for (auto& config : configs) {
        if (QaplaHelpers::to_lowercase(config.getName()) == QaplaHelpers::to_lowercase(name)) {
            return &config;
        }
    }
    return nullptr;
}

EngineConfig* EngineConfigManager::getConfigMutableByCmdAndProtocol(
    const std::string& cmd, EngineProtocol proto) 
{
    for (auto& config : configs) {
        if (config.getCmd() == cmd && config.getProtocol() == proto) {
            return &config;
        }
    }
    return nullptr;
}

void EngineConfigManager::addOrReplaceConfig(const EngineConfig& config, bool replaceOnDifferentProtocol) {
    for (auto& existing : configs) {
        if (existing.getCmd() == config.getCmd()) {
            if (existing.getProtocol() == config.getProtocol() || replaceOnDifferentProtocol) {
                existing = config;
                return;
            }
        }
    }
    configs.push_back(config);
}

std::unordered_set<std::string> EngineConfigManager::findMatchingNames(const std::vector<EngineConfig>& reference) const {
    std::unordered_set<std::string> valid;
    for (const auto& loaded : getAllConfigs()) {
        for (const auto& existing : reference) {
            if (loaded == existing) {
                valid.insert(loaded.getName());
                break;
            }
        }
    }
    return valid;
}

std::vector<std::string> collectDifferentiatingKeys(
    const std::vector<std::unordered_map<std::string, std::string>>& disambiguationMaps,
    size_t index, const std::vector<size_t>& indices)
{
    std::vector<std::string> differentiatingKeys;

    // Collect only keys that actually differentiate from other engines
    for (const auto& [key, value] : disambiguationMaps[index])
    {
        if (key == "name" || key == "trace" || key == "selected" || key == "gauntlet") {
            continue;
        }

        // Check if this key helps distinguish from any other engine
        for (std::size_t i : indices)
        {
            if (i == index) {
                continue; // Skip self
            }

            const auto& map = disambiguationMaps[i];
            auto it = map.find(key);
            if (it == map.end() || it->second != value)
            {
                differentiatingKeys.push_back(key);
                break;
            }
        }
    }

    return differentiatingKeys;
}

std::string buildDifferentiatorsForKey(
    const std::vector<std::unordered_map<std::string, std::string>>& disambiguationMaps,
    const std::string& key, const std::vector<size_t>& indices, size_t index,
    std::vector<std::string>& differentiators)
{
    size_t curIndex = 0;
    std::string currentEngineString;

    // Incrementally build all differentiators for this key
    for (std::size_t i : indices)
    {
        const auto& map = disambiguationMaps[i];
        auto it = map.find(key);
        if (it != map.end())
        {
            // Add key (and value if present) to this engine's differentiator string
            if (differentiators[curIndex].empty()) {
                differentiators[curIndex] += key;
            } else {
                differentiators[curIndex] += ", " + key;
            }
            if (!it->second.empty()) {
                differentiators[curIndex] += "=" + it->second;
            }
        }

        // Track the string for our target engine
        if (i == index) {
            currentEngineString = differentiators[curIndex];
        }

        curIndex++;
    }

    return currentEngineString;
}

bool isUniqueDifferentiator(const std::vector<std::string>& differentiators,
    const std::vector<size_t>& indices, size_t index, const std::string& currentEngineString)
{
    for (size_t i = 0; i < differentiators.size(); ++i)
    {
        if (indices[i] != index && differentiators[i] == currentEngineString)
        {
            return false;
        }
    }
    return true;
}

std::string computeUnifiedName(std::vector<std::unordered_map<std::string, std::string>> &disambiguationMaps, 
    size_t index, const std::vector<size_t> &indices)
{
    std::vector<std::string> differentiatingKeys = collectDifferentiatingKeys(disambiguationMaps, index, indices);
    
    // Progressive building: add one key at a time and check if it's sufficient
    std::vector<std::string> differentiators(indices.size());
    std::string currentEngineString;

    for (const std::string& key : differentiatingKeys)
    {
        currentEngineString = buildDifferentiatorsForKey(disambiguationMaps, key, indices, index, differentiators);
        
        if (isUniqueDifferentiator(differentiators, indices, index, currentEngineString))
        {
            break; // Found minimal set of keys that uniquely identify this engine
        }
    }
    
    return currentEngineString;
}

void EngineConfigManager::assignUniqueDisplayNames(std::vector<EngineConfig>& engines) {
    std::unordered_map<std::string, std::vector<std::size_t>> nameGroups;

    // Create disambiguation maps for all engines
    std::vector<std::unordered_map<std::string, std::string>> disambiguationMaps;
    disambiguationMaps.reserve(engines.size());
    for (const auto& engine : engines) {
        disambiguationMaps.push_back(engine.toDisambiguationMap());
    }

    // Group engines by base name
    for (std::size_t i = 0; i < disambiguationMaps.size(); ++i) {
        const auto& map = disambiguationMaps[i];
        auto it = map.find("name");
        const std::string& baseName = (it != map.end()) ? it->second : "unnamed";
        nameGroups[baseName].push_back(i);
    }

    // Assign unique names to engines with the same base name
    for (const auto& [baseName, indices] : nameGroups) {
        if (indices.size() == 1) {
            continue;
        }

        for (std::size_t index : indices) {
            std::string name = "[" + computeUnifiedName(disambiguationMaps, index, indices) + "]";

            if (name != "[]") {
                engines[index].setName(std::format("{} {}", baseName, name));
            }
        }
    }
}

} // namespace QaplaTester
