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

void EngineConfigManager::loadFromStream(std::istream& in) {

    errors.clear();

    while (in) {
        EngineConfig config;

        in >> config;
        addOrReplaceConfig(config);
    }
}


void EngineConfigManager::saveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) throw std::runtime_error("Unable to write file");

    for (const auto& config : configs) {
        file << config;
        file << "\n";
    }
}

std::vector<EngineConfig> EngineConfigManager::getAllConfigs() const {
    return configs;
}

const EngineConfig* EngineConfigManager::getConfig(const std::string& name) const {
    for (auto& config : configs) {
        if (to_lowercase(config.getName()) == to_lowercase(name)) return &config;
    }
    return nullptr;
}

EngineConfig* EngineConfigManager::getConfigMutable(const std::string& name)  {
    for (auto& config : configs) {
        if (to_lowercase(config.getName()) == to_lowercase(name)) return &config;
    }
    return nullptr;
}

void EngineConfigManager::addOrReplaceConfig(const EngineConfig& config) {
    for (auto& existing : configs) {
        if (existing.getName() == config.getName()) {
            existing = config;
            return;
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
