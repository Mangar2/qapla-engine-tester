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

#include "run-header-logger.h"
#include "../base-elements/logger.h"

#include <format>
#include <iomanip>
#include <sstream>

namespace QaplaTester {

namespace {

[[nodiscard]] bool isDefaultProperty(const std::string& key, const std::string& value) {
    if (key == "name" || key == "originalName") { return true; }
    if (key == "selected") { return true; }
    if (key == "dir" && value == ".") { return true; }
    if (key == "trace" && value == "none") { return true; }
    if (key == "restart" && value == "auto") { return true; }
    if ((key == "ponder" || key == "gauntlet" || key == "whitepov") && value == "false") { return true; }
    return false;
}

/**
 * @brief Logs "key: value" lines, column-aligned to the longest key in this block.
 *
 * Logger::logAligned uses a fixed 30-column width, which collides (no separating
 * space) whenever a key is longer than that. Run headers carry arbitrarily long,
 * descriptive keys (e.g. test names), so each block picks its own width instead.
 */
void logKeyValueBlock(Logger& logger, std::string_view indent,
    const std::vector<std::pair<std::string, std::string>>& lines) {

    size_t width = 0;
    for (const auto& [key, value] : lines) {
        width = std::max(width, key.size() + 1); // +1 for the trailing colon
    }

    for (const auto& [key, value] : lines) {
        std::ostringstream oss;
        oss << indent << std::left << std::setw(static_cast<int>(width) + 1) << (key + ":") << value;
        logger.log(oss.str(), TraceLevel::result);
    }
}

} // namespace

void RunHeaderLogger::logEngine(size_t index, const EngineConfig& engine) {
    auto& logger = Logger::reportLogger();
    logger.log(std::format("Engine {}: {}", index, engine.getName()), TraceLevel::result);

    std::vector<std::pair<std::string, std::string>> lines;
    engine.visitProperties([&lines](const std::string& key, const std::string& value) {
        if (value.empty() || isDefaultProperty(key, value)) { return; }
        lines.emplace_back(key, value);
    });
    for (const auto& [name, value] : engine.getOptionValues()) {
        lines.emplace_back(std::format("option {}", name), value);
    }

    logKeyValueBlock(logger, "    ", lines);
}

void RunHeaderLogger::log(std::string_view runType,
    const std::vector<EngineConfig>& engines,
    const std::vector<RunHeaderSetting>& settings) {

    auto& logger = Logger::reportLogger();
    logger.log(std::format("=== {} ===", runType), TraceLevel::result);

    if (!engines.empty()) {
        logger.log("Engines:", TraceLevel::result);
        for (size_t i = 0; i < engines.size(); ++i) {
            logEngine(i + 1, engines[i]);
        }
    }

    if (!settings.empty()) {
        logger.log("Settings:", TraceLevel::result);
        std::vector<std::pair<std::string, std::string>> lines;
        lines.reserve(settings.size());
        for (const auto& setting : settings) {
            lines.emplace_back(setting.key, setting.value);
        }
        logKeyValueBlock(logger, "    ", lines);
    }

    logger.log("", TraceLevel::result);
}

} // namespace QaplaTester
