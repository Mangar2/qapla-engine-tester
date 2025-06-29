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

InputHandler::CallbackRegistration::CallbackRegistration(InputHandler& handler, size_t id)
    : handler_(&handler), callbackId_(id) {
}

InputHandler::CallbackRegistration::~CallbackRegistration() {
    if (handler_) {
        handler_->unregisterCallback(callbackId_);
    }
}


void InputHandler::inputLoop(bool interactive) {
    static InputHandler inputHandler;
    if (inputHandler.started.exchange(true)) {
		throw std::runtime_error("InputHandler is already running");
    }
    setInstance(&inputHandler);
	if (!interactive) {
		// Non-interactive mode, no input thread needed
		return;
	}
    std::cout << "Interactive mode! Enter h or help for help, q or quit to quit" << std::endl;
    auto loop = [] {
        std::string line;
        while (!InputHandler::getInstance().quitRequested()) {
            if (!std::getline(std::cin, line)) break;
            InputHandler::getInstance().handleLine(line);
        }
        };

    inputHandler.inputThread = std::thread(loop);
    inputHandler.inputThread.detach();
}

void InputHandler::handleLine(const std::string& line) {
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    std::vector<std::string> args{ std::istream_iterator<std::string>{iss}, {} };
    try {
        if (command == "quit" || command == "q") {
            dispatchImmediate(ImmediateCommand::Quit, args);
            quitFlag = true;
        }
        else if (command == "set" || command == "s") handleSetCommand(args);
        else if (command == "info" || command == "?") dispatchImmediate(ImmediateCommand::Info, args);
        else if (command == "concurrency" || command == "c") dispatchImmediate(ImmediateCommand::Concurrency, args);
        else if (command == "abort" || command == "a") dispatchImmediate(ImmediateCommand::Abort, args);
        else if (command == "help" || command == "h") showHelp();
        else {
            std::cout << "Unknown command: " << command << "\n";
        }
    }
	catch (const std::exception& e) {
        std::cout << "Command failed: '" << command << "'. Reason: " << e.what() << "\n";
	}
}

void InputHandler::showHelp() {
    std::cout
        << "Available commands:\n"
        << "  quit | q           - Exit the program, waiting for current games to finish\n"
        << "  info | ?           - Show current engine/game state\n"
        << "  concurrency | c    - Set number of concurrent games\n"
        // << "  abort | a          - Abort current games immediately\n"
        << "  help | h           - Show this help message\n";
}


void InputHandler::handleSetCommand(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Usage: set <setting> <value>\n";
        return;
    }

    const std::string& key = args[0];
    const std::string& value = args[1];

    auto result = CliSettings::Manager::setGlobalValue(key, value);
    if (result.status != CliSettings::SetResult::Status::Success) {
        std::cout << "Error: " << result.errorMessage << "\n";
    }
}

void InputHandler::dispatchImmediate(ImmediateCommand cmd, const std::vector<std::string>& args) {
    CommandValue value;
    if (!args.empty())
        value = args[0];  

    for (const auto& entry : callbacks_) {
        if (std::ranges::find(entry.commands, cmd) != entry.commands.end())
            entry.callback(cmd, value);
    }
}

std::unique_ptr<InputHandler::CallbackRegistration>
InputHandler::registerCommandCallback(ImmediateCommand cmd, CommandCallback callback) {
    std::scoped_lock lock(callbacksMutex_);
    size_t id = nextCallbackId_++;
    callbacks_.emplace_back(CallbackEntry{ { cmd }, id, std::move(callback) });
    return std::make_unique<CallbackRegistration>(*this, id);
}

std::unique_ptr<InputHandler::CallbackRegistration>
InputHandler::registerCommandCallback(std::vector<ImmediateCommand> cmds, CommandCallback callback) {
    std::scoped_lock lock(callbacksMutex_);
    size_t id = nextCallbackId_++;
    callbacks_.emplace_back(CallbackEntry{ cmds, id, std::move(callback) });
    return std::make_unique<CallbackRegistration>(*this, id);
}


void InputHandler::unregisterCallback(size_t id) {
    std::scoped_lock lock(callbacksMutex_);
    std::erase_if(callbacks_, [&](const CallbackEntry& e) {
        return e.id == id;
        });
}