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
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#endif

EngineProcess::EngineProcess(const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& workingDir) {
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

    std::string cmd = path.string();

    BOOL success = CreateProcessA(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        workingDir ? workingDir->string().c_str() : nullptr,
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
        if (workingDir) chdir(workingDir->c_str());

        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(errPipe[1], STDERR_FILENO);

        close(inPipe[1]); close(outPipe[0]); close(errPipe[0]);

        execl(path.c_str(), path.c_str(), nullptr);
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
#ifdef _WIN32
    CloseHandle(stdinWrite_);
    CloseHandle(stdoutRead_);
    CloseHandle(stderrRead_);
    CloseHandle(childProcess_);
#else
    close(stdinWrite_);
    close(stdoutRead_);
    close(stderrRead_);
#endif
}

void EngineProcess::writeLine(const std::string& line) {
#ifdef _WIN32
    std::string withNewline = line + '\n';
    DWORD written;
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
        if (ch == '\n') break;
        line += ch;
    }
    return line;
}

std::optional<std::string> EngineProcess::readLine(std::chrono::milliseconds timeout) {
    return readLineImpl(
#ifdef _WIN32
        _open_osfhandle(reinterpret_cast<intptr_t>(stdoutRead_), 0),
#else
        stdoutRead_,
#endif
        timeout);
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

void EngineProcess::terminate() {
#ifdef _WIN32
    if (childProcess_) {
        BOOL result = TerminateProcess(childProcess_, 1);
        if (!result) {
            DWORD error = GetLastError();
            throw std::runtime_error("TerminateProcess failed with error code: " + std::to_string(error));
        }
    }
#else
    if (childPid_ > 0) {
        int killResult = kill(childPid_, SIGKILL);
        if (killResult == -1) {
            throw std::runtime_error("kill() failed");
        }

        int waitResult = waitpid(childPid_, nullptr, 0);
        if (waitResult == -1) {
            throw std::runtime_error("waitpid() failed");
        }
    }
#endif
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
