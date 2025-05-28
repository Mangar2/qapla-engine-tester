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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#include "engine-config-manager.h"

void EngineConfigManager::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) throw std::runtime_error("Unable to open file: " + filePath);

    errors.clear();

    while (file) {
        std::streampos startPos = file.tellg();
        EngineConfig config;

        try {
            file >> config;
            addOrUpdateConfig(config);
        }
        catch (const std::exception& e) {
            file.clear();
            file.seekg(startPos);
            std::string line;
            while (std::getline(file, line)) {
                if (line.empty()) continue;
                if (line[0] == '[' && line.back() == ']') {
                    file.seekg(-(std::streamoff)line.length() - 1, std::ios_base::cur);
                    break;
                }
            }
            errors.push_back(std::string("Parse error: ") + e.what());
        }
    }
}


void EngineConfigManager::saveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) throw std::runtime_error("Unable to write file");

    for (const auto& config : configs) {
        file << "[" << config.getName() << "]\n";
        file << config;
        file << "\n";
    }
}

std::vector<EngineConfig> EngineConfigManager::getAllConfigs() const {
    return configs;
}

EngineConfig* EngineConfigManager::getConfig(const std::string& name) {
    for (auto& config : configs) {
        if (config.getName() == name) return &config;
    }
    return nullptr;
}

void EngineConfigManager::addOrUpdateConfig(const EngineConfig& config) {
    for (auto& existing : configs) {
        if (existing.getName() == config.getName()) {
            existing = config;
            return;
        }
    }
    configs.push_back(config);
}

