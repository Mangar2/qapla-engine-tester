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

#include <sstream>
#include "input-handler.h"
#include "cli-settings-manager.h"


void InputHandler::inputLoop() {
    std::string line;
    while (!quitFlag) {
        if (!std::getline(std::cin, line)) break;
        handleLine(line);
    }
}

void InputHandler::handleLine(const std::string& line) {
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (command == "quit") {
        quitFlag = true;
    }
    else if (command == "set") {
        handleSetCommand(iss);
    }
    else if (command == "help") {
        CliSettings::Manager::showHelp();
    }
    else {
        std::cout << "Unknown command: " << command << "\n";
    }
}

void InputHandler::handleSetCommand(std::istringstream& iss) {
    std::string key, value;
    iss >> key >> value;

    if (key.empty() || value.empty()) {
        std::cout << "Usage: set <setting> <value>\n";
        return;
    }

    auto result = CliSettings::Manager::setGlobalValue(key, value);
    if (result.status != CliSettings::SetResult::Status::Success) {
        std::cout << "Error: " << result.errorMessage << "\n";
    }
}

void InputHandler::dispatchImmediate(ImmediateCommand cmd, CommandValue value) {
    std::scoped_lock lock(callbacksMutex_);
    auto it = commandCallbacks_.find(cmd);
    if (it != commandCallbacks_.end()) {
        for (const auto& cb : it->second) {
            cb(cmd, value);
        }
    }
}
