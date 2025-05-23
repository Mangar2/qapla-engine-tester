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

#include "engine-process.h"

#include <stdexcept>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#endif

#include "timer.h"

EngineProcess::EngineProcess(const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& workingDir,
    std::string identifier)
    : executablePath_(path), workingDirectory_(workingDir), identifier_(identifier) {
	if (!std::filesystem::exists(path)) {
		throw std::runtime_error("Engine executable not found: " + path.string());
	}
	if (!std::filesystem::is_regular_file(path)) {
		throw std::runtime_error("Engine path is not a regular file: " + path.string());
	}
	if (workingDir && !std::filesystem::exists(*workingDir)) {
		throw std::runtime_error("Working directory does not exist: " + workingDir->string());
	}
	start();
}

void EngineProcess::start() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE stdinReadTmp, stdoutWriteTmp, stderrWriteTmp;

    if (!CreatePipe(&stdinReadTmp, &stdinWrite_, &saAttr, 0) ||
        !SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error("Failed to create stdin pipe");
    }

    if (!CreatePipe(&stdoutRead_, &stdoutWriteTmp, &saAttr, 0) ||
        !SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error("Failed to create stdout pipe");
    }

    if (!CreatePipe(&stderrRead_, &stderrWriteTmp, &saAttr, 0) ||
        !SetHandleInformation(stderrRead_, HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error("Failed to create stderr pipe");
    }

    PROCESS_INFORMATION piProcInfo{};
    STARTUPINFOA siStartInfo{};
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdInput = stdinReadTmp;
    siStartInfo.hStdOutput = stdoutWriteTmp;
    siStartInfo.hStdError = stderrWriteTmp;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmd = executablePath_.string();

    BOOL success = CreateProcessA(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        workingDirectory_ ? workingDirectory_->string().c_str() : nullptr,
        &siStartInfo,
        &piProcInfo);

    CloseHandle(stdinReadTmp);
    CloseHandle(stdoutWriteTmp);
    CloseHandle(stderrWriteTmp);

    if (!success) {
        throw std::runtime_error("Failed to create process");
    }

    childProcess_ = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
#else
    int inPipe[2], outPipe[2], errPipe[2];
    if (pipe(inPipe) || pipe(outPipe) || pipe(errPipe)) {
        throw std::runtime_error("Failed to create pipes");
    }

    childPid_ = fork();
    if (childPid_ == -1) {
        throw std::runtime_error("Failed to fork process");
    }

    if (childPid_ == 0) {
        if (workingDirectory_) chdir(workingDirectory_->c_str());

        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(errPipe[1], STDERR_FILENO);

        close(inPipe[1]); close(outPipe[0]); close(errPipe[0]);

        execl(executablePath_.c_str(), executablePath_.c_str(), nullptr);
        _exit(1);
    }

    close(inPipe[0]); close(outPipe[1]); close(errPipe[1]);
    stdinWrite_ = inPipe[1];
    stdoutRead_ = outPipe[0];
    stderrRead_ = errPipe[0];
#endif
}


EngineProcess::~EngineProcess() {
    terminate();
}

void EngineProcess::closeAllHandles() {
#ifdef _WIN32
    if (stdinWrite_)   CloseHandle(stdinWrite_);
    if (stdoutRead_)   CloseHandle(stdoutRead_);
    if (stderrRead_)   CloseHandle(stderrRead_);
    if (childProcess_) CloseHandle(childProcess_);

    stdinWrite_ = 0;
    stdoutRead_ = 0;
    stderrRead_ = 0;
    childProcess_ = 0;
#else
    if (stdinWrite_ >= 0) close(stdinWrite_);
    if (stdoutRead_ >= 0) close(stdoutRead_);
    if (stderrRead_ >= 0) close(stderrRead_);

    stdinWrite_ = -1;
    stdoutRead_ = -1;
    stderrRead_ = -1;
#endif
}

int64_t EngineProcess::writeLine(const std::string& line) {
#ifdef _WIN32
    std::string withNewline = line + '\n';
    DWORD written;
	int64_t now = Timer::getCurrentTimeMs();
    if (!WriteFile(stdinWrite_, withNewline.c_str(), static_cast<DWORD>(withNewline.size()), &written, nullptr)) {
        throw std::runtime_error("Failed to write to stdin");
    }
#else
    std::string withNewline = line + '\n';
    ssize_t written = write(stdinWrite_, withNewline.c_str(), withNewline.size());
    if (written == -1) {
        throw std::runtime_error("Failed to write to stdin");
    }
#endif
    return now;
}

/**
 * Returns the current memory usage (in bytes) of the engine process.
 */
std::size_t EngineProcess::getMemoryUsage() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(childProcess_, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return pmc.PrivateUsage; // Private memory used by process
    }
    return 0;
#else
    // Linux version follows below
#endif
}

void EngineProcess::appendToLineQueue(const std::string& text, bool lineTerminated) {
    int64_t now = Timer::getCurrentTimeMs();

    if (!lineQueue_.empty() && !lineQueue_.back().complete) {
        lineQueue_.back().content += text;
		lineQueue_.back().complete = lineTerminated;
        return;
    }

    lineQueue_.emplace_back(EngineLine{ text, lineTerminated, now });
}

void EngineProcess::readFromPipeBlocking() {
    char temp[1024];
    int64_t now = Timer::getCurrentTimeMs();
    if (stdoutRead_ == 0) {
        return;
    }

#ifdef _WIN32
    DWORD bytesRead = 0;
    if (!ReadFile(stdoutRead_, temp, sizeof(temp), &bytesRead, nullptr) || bytesRead == 0) {
        return;
    }
    std::string incoming(temp, bytesRead);
#else
    ssize_t r = read(stdoutRead_, temp, sizeof(temp));
    if (r <= 0) return;
    std::string incoming(temp, r);
#endif

    size_t start = 0;
    size_t newline;
    while ((newline = incoming.find('\n', start)) != std::string::npos) {
        std::string line = incoming.substr(start, newline - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        appendToLineQueue(line, true);
        start = newline + 1;
    }

    if (start < incoming.size()) {
        std::string fragment = incoming.substr(start);
        appendToLineQueue(fragment, false);
    }
}

EngineLine EngineProcess::readLineBlocking() {
    bool read = false;
    while (true) {

        if (!lineQueue_.empty() && lineQueue_.front().complete) {
            EngineLine line = std::move(lineQueue_.front());
            lineQueue_.pop_front();
            if (std::all_of(line.content.begin(), line.content.end(), isspace)) {
                continue;
            }
            return line;
        }
#ifdef _WIN32
        if (stdoutRead_ == 0 || read) return EngineLine{ "", false, Timer::getCurrentTimeMs() };
#else
        if (stdoutRead_ < 0 || read) return EngineLine{ "", false, Timer::getCurrentTimeMs() };
#endif
        readFromPipeBlocking();
        read = true;
    }
}

std::optional<std::string> EngineProcess::readLineImpl(int fd, std::chrono::milliseconds timeout) {
    std::string line;
    char ch;
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return std::nullopt;

        int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());

#ifdef _WIN32
        DWORD bytesAvailable;
        if (!PeekNamedPipe(reinterpret_cast<HANDLE>(_get_osfhandle(fd)), nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            return std::nullopt;
        }
        if (bytesAvailable == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        DWORD read;
        if (!ReadFile(reinterpret_cast<HANDLE>(_get_osfhandle(fd)), &ch, 1, &read, nullptr) || read == 0) break;
#else
        struct pollfd pfd = { fd, POLLIN, 0 };
        int ret = poll(&pfd, 1, ms);
        if (ret <= 0) return std::nullopt;
        ssize_t r = read(fd, &ch, 1);
        if (r <= 0) break;
#endif
		if (ch == '\r') {
			continue; // Ignore carriage return
		}
        else if (ch == '\n') {
            if (std::all_of(line.begin(), line.end(), isspace)) {
                line.clear();
                continue;
            }
            break;
        }
        line += ch;
    }
    return line;
}

std::optional<std::string> EngineProcess::readErrorLine(std::chrono::milliseconds timeout) {
    return readLineImpl(
#ifdef _WIN32
        _open_osfhandle(reinterpret_cast<intptr_t>(stderrRead_), 0),
#else
        stderrRead_,
#endif
        timeout);
}

bool EngineProcess::waitForExit(std::chrono::milliseconds timeout) {
#ifdef _WIN32
    if (!childProcess_) return true;
    DWORD waitResult = WaitForSingleObject(childProcess_, static_cast<DWORD>(timeout.count()));
    if (waitResult == WAIT_OBJECT_0) {
        return true; // Process has exited
    }
    else if (waitResult == WAIT_TIMEOUT) {
        return false; // Still running
    }
    else {
        throw std::runtime_error("WaitForSingleObject failed");
    }
#else
    if (childPid_ <= 0) return true;

    int status = 0;
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        pid_t result = waitpid(childPid_, &status, WNOHANG);
        if (result > 0) {
            return true; // Prozess ist beendet
        }
        else if (result == 0) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false; // Timeout abgelaufen
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else {
            if (errno == EINTR) continue;
            throw std::runtime_error("waitpid failed");
        }
    }
#endif
}

/**
 * Terminates the engine process if it is still running.
 * If the process is already terminated, this is considered a successful outcome.
 * If termination fails, an exception is thrown.
 */
void EngineProcess::terminate() {
#ifdef _WIN32
    if (!childProcess_) {
        closeAllHandles();
        return; // Already terminated (positive case)
    }

    DWORD exitCode;
    if (GetExitCodeProcess(childProcess_, &exitCode)) {
        if (exitCode != STILL_ACTIVE) {
            closeAllHandles();
            return; // Process already exited
        }

        if (!TerminateProcess(childProcess_, 1)) {
            DWORD error = GetLastError();
            throw std::runtime_error("TerminateProcess failed with error code: " + std::to_string(error));
        }
    }
    else {
        DWORD error = GetLastError();
        throw std::runtime_error("GetExitCodeProcess failed with error code: " + std::to_string(error));
    }
#else
    if (childPid_ <= 0) {
        closeAllHandles();
        return; // Already terminated (positive case)
    }

    if (kill(childPid_, 0) == -1 && errno == ESRCH) {
        closeAllHandles();
        return; // Process no longer exists (positive case)
    }

    if (kill(childPid_, SIGKILL) == -1) {
        throw std::runtime_error("kill(SIGKILL) failed");
    }

    if (waitpid(childPid_, nullptr, 0) == -1) {
        throw std::runtime_error("waitpid() failed");
    }
#endif
    closeAllHandles();
    throw std::runtime_error("Engine did not end by itself");
}

void EngineProcess::restart() {
    try {
        terminate();
    }
	catch (...) {
		// We can ignore the error "Engine did not end by itself" error 
        // as the restart is already reported as an error
	}
	start();
}

bool EngineProcess::isRunning() const {
#ifdef _WIN32
    DWORD status;
    if (!GetExitCodeProcess(childProcess_, &status)) return false;
    return status == STILL_ACTIVE;
#else
    int status;
    return waitpid(childPid_, &status, WNOHANG) == 0;
#endif
}
