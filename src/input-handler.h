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
#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <mutex>
#include <iostream>
#include <functional>
#include <optional>
#include <variant>

 /**
  * @brief Handles asynchronous user input for runtime commands.
  *
  * Instantiate on stack and register with setInstance(&instance) for global access.
  */
class InputHandler {
public:
    enum class ImmediateCommand {
        Abort,
        Concurrency,
        Info,
        Quit,
        SetTraceLevel,       // Programmintern
        SetEngineTraceLevel  // Per Engine
    };

    class CallbackRegistration {
    public:
        ~CallbackRegistration();
        CallbackRegistration(InputHandler& handler, size_t id);

    private:
        InputHandler* handler_;
        size_t callbackId_;
    };

	using CommandValue = std::optional<std::string>;
    using CommandCallback = std::function<void(ImmediateCommand, CommandValue)>;

    /**
     * @brief Registers global access to a stack-allocated InputHandler instance.
     * @param instance Pointer to existing InputHandler.
     */
    static void setInstance(InputHandler* instance) {
        getGlobalInstance() = instance;
    }

    /**
     * @brief Returns the registered global instance.
     * @return Reference to the registered InputHandler.
     */
    static InputHandler& getInstance() {
        return *getGlobalInstance();
    }

    InputHandler() = default;

    /**
     * @brief Starts the input handling thread.
     */
    void start() {
        if (!inputThread.joinable()) {
            inputThread = std::thread(&InputHandler::inputLoop, this);
        }
    }

    /**
     * @brief Returns true if the user has requested to quit.
     * @return True if "quit" was entered.
     */
    bool quitRequested() const {
        return quitFlag.load();
    }

    /**
     * @brief Destructor stops the thread and cleans up.
     */
    ~InputHandler() {
        stopFlag = true;
        if (inputThread.joinable()) {
            inputThread.join();
        }
    }

    std::unique_ptr<CallbackRegistration> registerCommandCallback(ImmediateCommand cmd, CommandCallback callback);
    std::unique_ptr<CallbackRegistration> 
        registerCommandCallback(std::vector<ImmediateCommand> cmds, CommandCallback callback);

    void dispatchImmediate(ImmediateCommand cmd, const std::vector<std::string>& args);

private:
    void inputLoop();
    void handleSetCommand(const std::vector<std::string>& args);
    void handleLine(const std::string& line);

    static InputHandler*& getGlobalInstance() {
        static InputHandler* instance = nullptr;
        return instance;
    }

    std::atomic<bool> quitFlag{ false };
    std::atomic<bool> stopFlag{ false };
    std::thread inputThread;

    // Notification
    void unregisterCallback(size_t id);
    size_t nextCallbackId_ = 1;
    struct CallbackEntry {
        std::vector<ImmediateCommand> commands;
        size_t id;
        CommandCallback callback;
    };
    std::vector<CallbackEntry> callbacks_;
    std::mutex callbacksMutex_;
};
