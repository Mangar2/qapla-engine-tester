/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker BÃ¶hm
 * @copyright Copyright (c) 2025 Volker BÃ¶hm
 */

#pragma once

#include "../engine-handling\engine-config.h"
#include "../base-elements\time-control.h"
#include <vector>
#include <string>

namespace QaplaTester::Test {

    /**
     * @brief Parameters for creating test engines with sensible defaults
     */
    struct TestEngineParams {
        std::string name = "TestEngine";
        std::string cmd = "fictional_engine";
        std::string dir = "";
        std::string author = "Test Author";
        bool ponder = false;
        std::string timeControl = "depth=5";
        std::string traceLevel = "none";
        bool isGauntlet = false;
    };

    /**
     * @brief Creates a single engine configuration for testing
     * @param params Engine parameters with defaults
     * @return Configured EngineConfig
     */
    inline EngineConfig createEngine(const TestEngineParams& params = {}) {
        EngineConfig engine;
        engine.setName(params.name);
        engine.setCmd(params.cmd);
        if (!params.dir.empty()) {
            engine.setDir(params.dir);
        }
        engine.setAuthor(params.author);
        engine.setPonder(params.ponder);
        engine.setTimeControl(params.timeControl);
        engine.setTraceLevel(params.traceLevel);
        engine.setGauntlet(params.isGauntlet);
        return engine;
    }

    /**
     * @brief Creates multiple engine configurations for testing
     * @param paramsList Vector of engine parameters
     * @return Vector of configured EngineConfig instances
     */
    inline std::vector<EngineConfig> createEngines(const std::vector<TestEngineParams>& paramsList) {
        std::vector<EngineConfig> engines;
        engines.reserve(paramsList.size());
        for (const auto& params : paramsList) {
            engines.push_back(createEngine(params));
        }
        return engines;
    }

} // namespace QaplaTester::Test
