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

#include "mcp-server.h"
#include "../cli/settings-manager.h"

namespace QaplaTester::Mcp {

void McpServer::initialize() {
    silenceLoggers();
    // Additional modular initialization steps will be added here
}

void McpServer::silenceLoggers() {
    QaplaHelpers::ConfigData mcpConfig;
    QaplaHelpers::IniFile::Section loggingSection;
    loggingSection.name = "logging";
    loggingSection.addEntry("trace", "none");
    loggingSection.addEntry("engine", "false");
    mcpConfig.addSection(loggingSection);

    Settings::Manager::instance().parseInput(mcpConfig, true);
}

} // namespace QaplaTester::Mcp
