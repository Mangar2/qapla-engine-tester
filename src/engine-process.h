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
#include <deque>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

struct EngineLine {
    std::string content;
    bool complete;
    int64_t timestampMs; 
};

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
	 * @brief Closes the engine process handles and releases resources.
     */
    void closeAllHandles();

    /**
     * @brief Sends a single line to the engine's stdin.
     * @param line Line to send (without newline).
     * @throws std::runtime_error if writing fails.
     * @returns timestamp when the data has been written
     */
    int64_t writeLine(const std::string& line);

    /**
     * @brief Reads a single line from stdout with a timeout.
     * @param timeout Duration to wait before giving up.
     * @return Line from stdout, or std::nullopt on timeout.
     * @throws std::runtime_error if reading fails.
     */
    std::optional<std::string> readLine(std::chrono::milliseconds timeout);

    /**
     * Blocks until a complete line from the engine has been read and returns it with timestamp.
     *
     * If no complete line is currently available, the method continues reading from the pipe
     * until one is. Lines are read and timestamped in readFromPipe().
     *
     * @return An EngineLine containing the line content, timestamp, and completeness flag.
     */
    EngineLine readLineBlocking();

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

    /**
     * Appends a line or line fragment to the line queue with timestamp.
     *
     * If the last entry is incomplete and this is a continuation (lineTerminated = false),
     * the content is appended to the existing entry. Otherwise, a new entry is created.
     *
     * @param text The text to add.
     * @param lineTerminated True if the text ends with a line break (complete line).
     */
    void appendToLineQueue(const std::string& text, bool lineTerminated);

    /**
     * Reads a block of raw bytes from the engine's stdout pipe and splits them into lines.
     *
     * Each complete line (ending with '\n') is stored in lineQueue_ with a timestamp.
     * The last partial line, if any, is also stored but marked as incomplete.
     * This method does not block if data is not immediately available.
     */
    void readFromPipe();
    std::deque<EngineLine> lineQueue_;

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
