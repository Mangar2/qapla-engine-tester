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

InputHandler::CallbackRegistration::CallbackRegistration(InputHandler& handler, ImmediateCommand command, size_t id)
    : handler_(&handler), command_(command), callbackId_(id) {
}

InputHandler::CallbackRegistration::~CallbackRegistration() {
    if (handler_) {
        handler_->unregisterCallback(command_, callbackId_);
    }
}


void InputHandler::inputLoop() {
    std::string line;
    while (!quitFlag) {
        if (!std::getline(std::cin, line)) break;
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
		//line = "?"; // Simulate input for testing purposes
        handleLine(line);
    }
}

void InputHandler::handleLine(const std::string& line) {
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (command == "quit" || command == "q")  quitFlag = true;
    else if (command == "set" || command == "s") handleSetCommand(iss);
    else if (command == "info" || command == "?") dispatchImmediate(ImmediateCommand::Info);
    else if (command == "help" || command == "h") CliSettings::Manager::showHelp();
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
    for (const auto& entry : callbacks_) {
        if (entry.command == cmd)
            entry.callback(cmd, value);
    }
}


std::unique_ptr<InputHandler::CallbackRegistration>
InputHandler::registerCommandCallback(ImmediateCommand cmd, CommandCallback callback) {
    std::scoped_lock lock(callbacksMutex_);
    size_t id = nextCallbackId_++;
    callbacks_.emplace_back(CallbackEntry{ cmd, id, std::move(callback) });
    return std::make_unique<CallbackRegistration>(*this, cmd, id);
}

void InputHandler::unregisterCallback(ImmediateCommand cmd, size_t id) {
    std::scoped_lock lock(callbacksMutex_);
    std::erase_if(callbacks_, [&](const CallbackEntry& e) {
        return e.command == cmd && e.id == id;
        });
}