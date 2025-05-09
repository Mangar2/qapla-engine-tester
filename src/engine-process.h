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

#include <string>
#include <filesystem>
#include <optional>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

 /**
  * @brief Manages the lifecycle and communication of an external engine process.
  *
  * Responsible for starting the process, providing communication via stdin/stdout,
  * handling timeouts, and ensuring proper termination across platforms.
  */
class EngineProcess {
public:
    /**
     * @brief Constructs and starts the engine process.
     * @param executablePath Path to the engine executable.
     * @param workingDirectory Optional working directory for the process.
     * @throws std::runtime_error if the process cannot be started.
     */
    EngineProcess(const std::filesystem::path& executablePath,
        const std::optional<std::filesystem::path>& workingDirectory = std::nullopt);

    /**
     * @brief Destructor ensures the process is safely terminated.
     */
    ~EngineProcess();

    EngineProcess(const EngineProcess&) = delete;
    EngineProcess& operator=(const EngineProcess&) = delete;

    /**
     * @brief Sends a single line to the engine's stdin.
     * @param line Line to send (without newline).
     * @throws std::runtime_error if writing fails.
     */
    void writeLine(const std::string& line);

    /**
     * @brief Reads a single line from stdout with a timeout.
     * @param timeout Duration to wait before giving up.
     * @return Line from stdout, or std::nullopt on timeout.
     * @throws std::runtime_error if reading fails.
     */
    std::optional<std::string> readLine(std::chrono::milliseconds timeout);

    /**
     * @brief Attempts to read a single line from stdout without blocking.
     * @return Line from stdout if available immediately, or std::nullopt if no data is ready.
     * @throws std::runtime_error if reading fails.
     */
    inline std::optional<std::string> tryReadLine() {
        return readLine(std::chrono::milliseconds(0));
    }

    /**
     * @brief Reads a single line from stderr with a timeout.
     * @param timeout Duration to wait before giving up.
     * @return Line from stderr, or std::nullopt on timeout.
     * @throws std::runtime_error if reading fails.
     */
    std::optional<std::string> readErrorLine(std::chrono::milliseconds timeout);

    /**
     * @brief Waits for the engine process to exit within the given timeout.
     * @param milliseconds Timeout in milliseconds to wait.
     * @return true if the process exited within the timeout; false otherwise.
     */
    bool waitForExit(std::chrono::milliseconds timeout);

    /**
     * @brief Forcefully terminates the engine process.
     *        Can be called manually before destruction.
     */
    void terminate();

    /**
     * @brief Checks if the engine process is still running.
     * @return true if the process is alive.
     */
    bool isRunning() const;

private:
    mutable std::string stdoutBuffer_;
    std::optional<std::string> readLineImpl(int fd, std::chrono::milliseconds timeout);

#ifdef _WIN32
    HANDLE childProcess_ = nullptr;
    HANDLE stdinWrite_ = nullptr;
    HANDLE stdoutRead_ = nullptr;
    HANDLE stderrRead_ = nullptr;
#else
    pid_t childPid_ = -1;
    int stdinWrite_ = -1;
    int stdoutRead_ = -1;
    int stderrRead_ = -1;
#endif
};
