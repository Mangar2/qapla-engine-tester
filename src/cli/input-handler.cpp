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
#include <iterator>
#include <ranges>   
#include <vector>

#include "input-handler.h"
#include "cli-settings-manager.h"

namespace QaplaTester {

InputHandler::CallbackRegistration::CallbackRegistration(InputHandler& handler, size_t id)
    : handler_(&handler), callbackId_(id) {
}

InputHandler::CallbackRegistration::~CallbackRegistration() {
    if (handler_ != nullptr) {
        handler_->unregisterCallback(callbackId_);
    }
}


void InputHandler::inputLoop(bool interactive) {
    if (InputHandler::getInstance().started.exchange(true)) {
        throw std::runtime_error("InputHandler is already running");
    }
	if (!interactive) {
		// Non-interactive mode, no input thread needed
		return;
	}
    std::cout << "Interactive mode! Enter h or help for help, q or quit to quit\n";
    auto loop = [] {
        std::string line;
        while (!InputHandler::getInstance().quitRequested()) {
            if (!std::getline(std::cin, line)) {
                break;
            }
            InputHandler::getInstance().handleLine(line);
        }
        };

    InputHandler::getInstance().inputThread = std::thread(loop);
}

void InputHandler::handleLine(const std::string& line) {  // NOLINT(readability-function-cognitive-complexity)
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    std::vector<std::string> args{ std::istream_iterator<std::string>{iss}, {} };
    try {
        if (command == "quit" || command == "q") {
            dispatchImmediate(ImmediateCommand::Quit, args);
            quitFlag = true;
        }
        else if (command == "abort" || command == "a") {
            dispatchImmediate(ImmediateCommand::Abort, args);
            quitFlag = true;
        }
        else if (command == "concurrency" || command == "c") {
            dispatchImmediate(ImmediateCommand::Concurrency, args);
        }
        else if (command == "help" || command == "h") {
            showHelp();
        }
        else if (command == "info" || command == "?" || command == "i") {
            dispatchImmediate(ImmediateCommand::Info, args);
        }
        else if (command == "leaveinput" || command == "l") {
            quitFlag = true;
        }
        else if (command == "outcome" || command == "o") {
            dispatchImmediate(ImmediateCommand::Outcome, args);
        }
        else if (command == "pause" || command == "p") {
            dispatchImmediate(ImmediateCommand::Pause, args);
        }
        else if (command == "running" || command == "r") {
            dispatchImmediate(ImmediateCommand::Running, args);
        }
        else if (command == "set" || command == "s") {
            handleSetCommand(args);
        }
        else if (command == "setenginetracelevel" || command == "setel") {
            dispatchImmediate(ImmediateCommand::SetEngineTraceLevel, args);
        }
        else if (command == "settracelevel" || command == "stl") {
            dispatchImmediate(ImmediateCommand::SetTraceLevel, args);
        }
        else if (command == "viewgame" || command == "v") {
            dispatchImmediate(ImmediateCommand::ViewGame, args);
        }
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
        << "  help | h           - Show this help message\n"
        << "  abort | a          - Abort current games immediately\n"
        << "  concurrency | c    - Set number of concurrent games\n"
        << "  info | i | ?       - Show current engine/game state\n"
        << "  leaveinput | l     - Leave interactive mode; program keeps running\n"
        << "  outcome | o        - Show engine win/draw/loss causes\n"
		<< "  pause | p          - Pause all running games\n"
        << "  running | r        - Show all currently running game pairings\n"
        << "  viewgame | v       - Show UCI/Winboard log of a specific game by ID\n";
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
    if (!args.empty()) {
        value = args[0];
    }

    for (const auto& entry : callbacks_) {
        if (std::ranges::find(entry.commands, cmd) != entry.commands.end()) {
            entry.callback(cmd, value);
        }
    }
}

std::unique_ptr<InputHandler::CallbackRegistration>
InputHandler::registerCommandCallback(ImmediateCommand cmd, CommandCallback callback) {
    std::scoped_lock lock(callbacksMutex_);
    size_t id = nextCallbackId_++;
    callbacks_.emplace_back(CallbackEntry{ .commands={ cmd }, .id=id, .callback=std::move(callback) });
    return std::make_unique<CallbackRegistration>(*this, id);
}

std::unique_ptr<InputHandler::CallbackRegistration>
InputHandler::registerCommandCallback(std::vector<ImmediateCommand> cmds, CommandCallback callback) {
    std::scoped_lock lock(callbacksMutex_);
    size_t id = nextCallbackId_++;
    callbacks_.emplace_back(CallbackEntry{ .commands=std::move(cmds), .id=id, .callback=std::move(callback) });
    return std::make_unique<CallbackRegistration>(*this, id);
}


void InputHandler::unregisterCallback(size_t id) {
    std::scoped_lock lock(callbacksMutex_);
    std::erase_if(callbacks_, [&](const CallbackEntry& e) {
        return e.id == id;
        });
}

} // namespace QaplaTester
