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
#include <string>
#include <assert.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Psapi.h>
#include <io.h>
#else
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <fstream>
#include <sstream>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

#include "timer.h"
#include "logger.h"

#ifdef _WIN32
struct EngineProcess::Win32IoData {
    OVERLAPPED overlappedRead{};
    OVERLAPPED overlappedWrite{};
    HANDLE writeEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    HANDLE readEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    ~Win32IoData() { 
        if (readEvent) CloseHandle(readEvent); 
        if (writeEvent) CloseHandle(writeEvent);
    }
};
#endif

EngineProcess::EngineProcess(const std::filesystem::path &path,
                             const std::optional<std::filesystem::path> &workingDir,
                             std::string identifier)
    : executablePath_(path), workingDirectory_(workingDir), identifier_(identifier)
{
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("Engine executable not found: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Engine path is not a regular file: " + path.string());
    }
    if (workingDir && !std::filesystem::exists(*workingDir))
    {
        throw std::runtime_error("Working directory does not exist: " + workingDir->string());
    }
    start();
}

void EngineProcess::startWin32() {
#ifdef _WIN32
    constexpr bool useStdErr = false;
    constexpr DWORD READ_PUFFER_SIZE = 64 * 1024;

    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE stdinReadTmp, stdoutWriteTmp, stderrWriteTmp;
    HANDLE nulHandle = nullptr;

    if (!CreatePipe(&stdinReadTmp, &stdinWrite_, &saAttr, 0) ||
        !SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0))
    {
        throw std::runtime_error("Failed to create stdin pipe");
    }

    if (!CreatePipe(&stdoutRead_, &stdoutWriteTmp, &saAttr, READ_PUFFER_SIZE) ||
        !SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0))
    {
        throw std::runtime_error("Failed to create stdout pipe");
    }

    if (useStdErr)
    {
        if (!CreatePipe(&stderrRead_, &stderrWriteTmp, &saAttr, 0) ||
            !SetHandleInformation(stderrRead_, HANDLE_FLAG_INHERIT, 0))
        {
            throw std::runtime_error("Failed to create stderr pipe");
        }
    }
    else
    {
        nulHandle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (nulHandle == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("Failed to open NUL device for stderr redirection");
        }
    }

    PROCESS_INFORMATION piProcInfo{};
    STARTUPINFOA siStartInfo{};
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdInput = stdinReadTmp;
    siStartInfo.hStdOutput = stdoutWriteTmp;
    siStartInfo.hStdError = useStdErr ? stderrWriteTmp : nulHandle;
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
    if (useStdErr)
    {
        CloseHandle(stderrWriteTmp);
    }
    else
    {
        CloseHandle(nulHandle);
    }

    if (!success)
    {
        throw std::runtime_error("Failed to create process");
    }

    childProcess_ = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
#endif
}

void EngineProcess::startWin32Overlapped() {
#ifdef _WIN32

    constexpr bool useStdErr = false;
    constexpr DWORD READ_PUFFER_SIZE = 64 * 1024;

    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE stdinReadTmp, stdoutWriteTmp, stderrWriteTmp;
    HANDLE nulHandle = nullptr;

    if (!CreatePipe(&stdinReadTmp, &stdinWrite_, &saAttr, 0) ||
        !SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0))
    {
        throw std::runtime_error("Failed to create stdin pipe");
    }

    if (!CreatePipe(&stdoutRead_, &stdoutWriteTmp, &saAttr, READ_PUFFER_SIZE) ||
        !SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0))
    {
        throw std::runtime_error("Failed to create stdout pipe");
    }

    if (useStdErr)
    {
        if (!CreatePipe(&stderrRead_, &stderrWriteTmp, &saAttr, 0) ||
            !SetHandleInformation(stderrRead_, HANDLE_FLAG_INHERIT, 0))
        {
            throw std::runtime_error("Failed to create stderr pipe");
        }
    }
    else
    {
        nulHandle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (nulHandle == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("Failed to open NUL device for stderr redirection");
        }
    }

    PROCESS_INFORMATION piProcInfo{};
    STARTUPINFOA siStartInfo{};
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdInput = stdinReadTmp;
    siStartInfo.hStdOutput = stdoutWriteTmp;
    siStartInfo.hStdError = useStdErr ? stderrWriteTmp : nulHandle;
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
    if (useStdErr)
    {
        CloseHandle(stderrWriteTmp);
    }
    else
    {
        CloseHandle(nulHandle);
    }

    if (!success)
    {
        throw std::runtime_error("Failed to create process");
    }

    childProcess_ = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
    win32IoData_ = std::make_unique<Win32IoData>();

#endif
}


void EngineProcess::start()
{
#ifdef _WIN32
    startWin32Overlapped();
#else
    int inPipe[2], outPipe[2], errPipe[2], execStatusPipe[2];
    if (pipe(inPipe) || pipe(outPipe) || pipe(execStatusPipe))
    {
        throw std::runtime_error("Failed to create pipes");
    }
    fcntl(execStatusPipe[1], F_SETFD, FD_CLOEXEC);

    if (useStdErr && pipe(errPipe))
    {
        throw std::runtime_error("Failed to create stderr pipe");
    }

    childPid_ = fork();
    if (childPid_ == -1)
    {
        throw std::runtime_error("Failed to fork process");
    }
    
    if (childPid_ == 0)
    {
        #ifdef __linux__
        // Set the parent death signal to SIGKILL
        // This ensures the child process is killed if the parent dies unexpectedly
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        #endif
        if (workingDirectory_ && chdir(workingDirectory_->c_str()) == -1) {
            perror("chdir failed");
        }            

        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        if (useStdErr)
        {
            dup2(errPipe[1], STDERR_FILENO);
            close(errPipe[0]);
        }
        else
        {
            int nulFd = open("/dev/null", O_WRONLY);
            if (nulFd != -1)
            {
                dup2(nulFd, STDERR_FILENO);
                close(nulFd);
            }
        }

        close(inPipe[1]);
        close(outPipe[0]);
        close(execStatusPipe[0]); // close unused read end
        // Signal exec error to parent if execl fails

        execl(executablePath_.c_str(), executablePath_.c_str(), nullptr);

        // only on failure:
        int err = errno;
        ssize_t w = write(execStatusPipe[1], &err, sizeof(err));
        if (w != sizeof(err)) {
            // write failed, we can't do much about it
        }
        _exit(1);
    }

    // Parent
    close(inPipe[0]);
    close(outPipe[1]);
    stdinWrite_ = inPipe[1];
    stdoutRead_ = outPipe[0];
    if (useStdErr)
    {
        close(errPipe[1]);
        stderrRead_ = errPipe[0];
    }
    close(execStatusPipe[1]); // close unused write end

    int execError = 0;
    ssize_t n = read(execStatusPipe[0], &execError, sizeof(execError));
    close(execStatusPipe[0]);

    if (n > 0)
    {
        // Child reported exec failure
        std::string msg = "Failed to exec engine process: ";
        msg += strerror(execError);
        throw std::runtime_error(msg);
    }
#endif
}

EngineProcess::~EngineProcess()
{
    terminate();
}

void EngineProcess::closeAllHandles()
{
#ifdef _WIN32
    if (stdinWrite_)
        CloseHandle(stdinWrite_);
    stdinWrite_ = nullptr;

    if (stdoutRead_)
        CloseHandle(stdoutRead_);
    stdoutRead_ = nullptr;

    if (stderrRead_)
        CloseHandle(stderrRead_);
    stderrRead_ = nullptr;

    if (childProcess_)
        CloseHandle(childProcess_);
    childProcess_ = nullptr;
#else
    if (stdinWrite_ >= 0)
        close(stdinWrite_);
    stdinWrite_ = -1;

    if (stdoutRead_ >= 0)
        close(stdoutRead_);
    stdoutRead_ = -1;

    if (stderrRead_ >= 0)
        close(stderrRead_);
    stderrRead_ = -1;

#endif
}

void EngineProcess::writeLineOverlapped(const std::string& withNewline)
{
#ifdef _WIN32
    constexpr DWORD writeTimeoutMs = 500;
    auto& overlapped = win32IoData_->overlappedWrite;
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    ResetEvent(win32IoData_->writeEvent);
    overlapped.hEvent = win32IoData_->writeEvent;

    if (!WriteFile(stdinWrite_, withNewline.c_str(), static_cast<DWORD>(withNewline.size()), nullptr, &overlapped)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            throw std::runtime_error("Failed to initiate overlapped write");
        }

        DWORD bytesWritten = 0;
        if (WaitForSingleObject(win32IoData_->writeEvent, writeTimeoutMs) != WAIT_OBJECT_0 ||
            !GetOverlappedResult(stdinWrite_, &overlapped, &bytesWritten, FALSE)) {
            throw std::runtime_error("Failed to complete overlapped write");
        }
    }
#endif
}

uint64_t EngineProcess::writeLine(const std::string &line)
{
    uint64_t now = Timer::getCurrentTimeMs();
    std::string withNewline = line + '\n';
#ifdef _WIN32
    writeLineOverlapped(withNewline);
    /*
    DWORD written;
    if (!WriteFile(stdinWrite_, withNewline.c_str(), static_cast<DWORD>(withNewline.size()), &written, nullptr))
    {
        throw std::runtime_error("Failed to write to stdin");
    }
    */
#else
    ssize_t written = write(stdinWrite_, withNewline.c_str(), withNewline.size());
    if (written == -1)
    {
        throw std::runtime_error("Failed to write to stdin");
    }
#endif
    return now;
}

/**
 * Returns the current memory usage (in bytes) of the engine process.
 */
std::size_t EngineProcess::getMemoryUsage() const
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(childProcess_, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc)))
    {
        // return pmc.PrivateUsage; // Pages
        return pmc.WorkingSetSize; // RAM
    }
    return 0;
#elif defined(__APPLE__)
    std::string cmd = "ps -o rss= -p " + std::to_string(childPid_);
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return 0;
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        pclose(pipe);
        return std::stoul(buffer) * 1024; // KB → Bytes
    }
    pclose(pipe);
    return 0;
#else
    std::ifstream statusFile("/proc/" + std::to_string(childPid_) + "/status");
    std::string line;
    while (std::getline(statusFile, line))
    {
        if (line.rfind("VmRSS:", 0) == 0)
        {
            std::istringstream iss(line);
            std::string key;
            std::size_t valueKb;
            std::string unit;
            if (iss >> key >> valueKb >> unit)
            {
                return valueKb * 1024; // Convert KB to Bytes
            }
        }
    }
    return 0;
#endif
}

void EngineProcess::appendErrorToLineQueue(EngineLine::Error error, const std::string &text)
{
    uint64_t now = Timer::getCurrentTimeMs();
    if (error == EngineLine::Error::NoError)
    {
        assert(error != EngineLine::Error::NoError && "appendErrorToLineQueue called with NoError");
        return;
    }

    if (!lineQueue_.empty() && !lineQueue_.back().complete)
    {
        lineQueue_.back().complete = true;
        lineQueue_.back().error = EngineLine::Error::IncompleteLine;
    }
    lineQueue_.emplace_back(EngineLine{text, true, now, error});
}

void EngineProcess::appendToLineQueue(const std::string &text, bool lineTerminated)
{
    uint64_t now = Timer::getCurrentTimeMs();

    if (!lineQueue_.empty() && !lineQueue_.back().complete)
    {
        lineQueue_.back().content += text;
        lineQueue_.back().complete = lineTerminated;
        return;
    }

    lineQueue_.emplace_back(EngineLine{text, lineTerminated, now, EngineLine::Error::NoError});
}

EngineProcess::ReadResult EngineProcess::readFromStdOutOverlapped() {
#ifdef _WIN32
    constexpr DWORD readTimeoutMs = 500;

    ReadResult result{};
    result.errorCode = 0;
    result.success = false;

    auto& overlapped = win32IoData_->overlappedRead;
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    ResetEvent(win32IoData_->readEvent);
    overlapped.hEvent = win32IoData_->readEvent;

    BOOL readResult = ReadFile(stdoutRead_, result.buffer.data(), static_cast<DWORD>(result.buffer.size()), nullptr, &overlapped);
    if (!readResult && GetLastError() != ERROR_IO_PENDING) {
        result.errorCode = GetLastError();
        return result;
    }

    DWORD waitResult = WaitForSingleObject(win32IoData_->readEvent, readTimeoutMs); 
    if (waitResult != WAIT_OBJECT_0) {
        CancelIoEx(stdoutRead_, &overlapped);
        result.errorCode = WAIT_TIMEOUT;
        return result;
    }

    DWORD bytesTransferred = 0;
    if (!GetOverlappedResult(stdoutRead_, &overlapped, &bytesTransferred, FALSE)) {
        result.errorCode = GetLastError();
        return result;
    }

    result.bytesRead = bytesTransferred;
    result.success = true;
    return result;
#else
    return {};
#endif
}



void EngineProcess::readFromPipeBlocking()
{
    if (stdoutRead_ == nullptr)
    {
        return;
    }
    size_t bytesRead = 0;

#ifdef _WIN32
    // DWORD winBytesRead = 0;
    //if (!ReadFile(stdoutRead_, temp, sizeof(temp), &winBytesRead, nullptr) || winBytesRead == 0)
    auto result = readFromStdOutOverlapped();
    if (!result.success || result.bytesRead == 0)
    {
        //DWORD lastError = GetLastError();
		DWORD lastError = result.errorCode;
        // std::cout << "[" << now << "] " << "lastError: " << lastError << " winBytesRead: " << winBytesRead << std::endl;
        switch (lastError)
        {
        case ERROR_BROKEN_PIPE:
            appendErrorToLineQueue(EngineLine::Error::EngineTerminated, "ReadFile: Broken pipe - engine terminated or closed");
            break;
        case ERROR_NO_DATA:
            appendErrorToLineQueue(EngineLine::Error::EngineTerminated, "ReadFile: No data - end of file");
            break;
        case ERROR_INVALID_HANDLE:
            appendErrorToLineQueue(EngineLine::Error::EngineTerminated, "ReadFile: Invalid handle");
            break;
        default:
            appendErrorToLineQueue(EngineLine::Error::EngineTerminated, "ReadFile: Unknown error code " + std::to_string(lastError));
            break;
        }
        return;
    }
    //bytesRead = static_cast<size_t>(winBytesRead);
	bytesRead = result.bytesRead;
    const char* temp = result.buffer.data();
    // std::cout << "[" << now << "] " << incoming << std::endl;
#else
    char temp[1024];

    ssize_t linuxBytesRead = read(stdoutRead_, temp, sizeof(temp));
    if (linuxBytesRead == 0)
    {
        appendErrorToLineQueue(EngineLine::Error::EngineTerminated, "read: EOF - engine closed pipe");
        return;
    }
    if (linuxBytesRead < 0)
    {
        if (errno == EBADF)
        {
            appendErrorToLineQueue(EngineLine::Error::EngineTerminated, "read: Invalid file descriptor");
        }
        else
        {
            appendErrorToLineQueue(EngineLine::Error::EngineTerminated, std::string("read: error ") + strerror(errno));
        }
        return;
    }
    bytesRead = static_cast<size_t>(linuxBytesRead);
#endif

    size_t start = 0;
    for (size_t i = 0; i < bytesRead; ++i)
    {
        if (temp[i] == '\n')
        {
            size_t len = i - start;
            if (len > 0 && temp[i - 1] == '\r')
            {
                --len;
            }
            std::string line(temp + start, len);
            appendToLineQueue(line, true);
            start = i + 1;
        }
    }

    if (start < bytesRead)
    {
        std::string fragment(temp + start, bytesRead - start);
        appendToLineQueue(fragment, false);
    }
}

EngineLine EngineProcess::readLineBlocking()
{
    bool read = false;
    while (true)
    {

        if (!lineQueue_.empty() && lineQueue_.front().complete)
        {
            auto line = std::move(lineQueue_.front());
            lineQueue_.pop_front();
            if (std::find_if_not(
                line.content.begin(),
                line.content.end(),
                [](unsigned char ch) { return std::isspace(ch); }) != line.content.end())
            {
                return line;
            }
            continue;
        }
#ifdef _WIN32
        if (stdoutRead_ == nullptr || read)
            return EngineLine{"", false, Timer::getCurrentTimeMs()};
#else
        if (stdoutRead_ < 0 || read)
            return EngineLine{"", false, Timer::getCurrentTimeMs()};
#endif
        reading_ = true;
        if (!terminating_)
        {
            readFromPipeBlocking();
        }
        reading_ = false;

        read = true;
    }
}

bool EngineProcess::waitForExit(std::chrono::milliseconds timeout)
{
#ifdef _WIN32
    if (!childProcess_)
        return true;
    DWORD waitResult = WaitForSingleObject(childProcess_, static_cast<DWORD>(timeout.count()));
    if (waitResult == WAIT_OBJECT_0)
    {
        return true; // Process has exited
    }
    else if (waitResult == WAIT_TIMEOUT)
    {
        return false; // Still running
    }
    else
    {
        throw std::runtime_error("WaitForSingleObject failed");
    }
#else
    if (childPid_ <= 0)
        return true;

    int status = 0;
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true)
    {
        pid_t result = waitpid(childPid_, &status, WNOHANG);
        if (result > 0)
        {
            return true; // Prozess ist beendet
        }
        else if (result == 0)
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return false; // Timeout abgelaufen
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else
        {
            if (errno == ECHILD)
            {
                return true; // No child process, considered as exited
            }
            if (errno == EINTR)
                continue;
            throw std::runtime_error("waitpid() failed " + std::string(std::strerror(errno)));
        }
    }
#endif
}

/**
 * Terminates the engine process if it is still running.
 * If the process is already terminated, this is considered a successful outcome.
 * If termination fails, an exception is thrown.
 */
void EngineProcess::terminate()
{
#ifdef _WIN32
    if (!childProcess_)
    {
        closeAllHandles();
        return; // Already terminated (positive case)
    }
    // Ensures that the read thread will not continue to start reading.
    terminating_ = true;
    // Loop until we are sure reading is stopped.    
    while (reading_)
    {
        CancelIoEx(stdoutRead_, &win32IoData_->overlappedRead);
        CancelIoEx(stdinWrite_, &win32IoData_->overlappedWrite);
        SetEvent(win32IoData_->readEvent);
        SetEvent(win32IoData_->writeEvent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    DWORD exitCode;
    if (GetExitCodeProcess(childProcess_, &exitCode))
    {
        if (exitCode != STILL_ACTIVE)
        {
            closeAllHandles();
            return; // Process already exited
        }

        if (!TerminateProcess(childProcess_, 1))
        {
            DWORD error = GetLastError();
            throw std::runtime_error("TerminateProcess failed with error code: " + std::to_string(error));
        }
    }
    else
    {
        DWORD error = GetLastError();
        throw std::runtime_error("GetExitCodeProcess failed with error code: " + std::to_string(error));
    }
    bool exited = waitForExit(std::chrono::seconds(5)); 
    closeAllHandles();
    // Should never be reached unless TerminateProcess succeeds but the process remains active (unexpected)
    if (!exited) {
        throw std::runtime_error("Engine did not end by itself");
    }
#else
    if (childPid_ <= 0)
    {
        closeAllHandles();
        return; // Already terminated (positive case)
    }

    if (kill(childPid_, 0) == -1 && errno == ESRCH)
    {
        closeAllHandles();
        return; // Process no longer exists (positive case)
    }

    if (kill(childPid_, SIGKILL) == -1)
    {
        throw std::runtime_error("kill(SIGKILL) failed");
    }

    bool exited = waitForExit(std::chrono::seconds(5)); // Wait for process to exit
    closeAllHandles();
    if (!exited)
    {
        throw std::runtime_error("Engine did not end by itself");
    }
#endif
}

bool EngineProcess::isRunning() const
{
#ifdef _WIN32
    DWORD status;
    if (!GetExitCodeProcess(childProcess_, &status))
        return false;
    return status == STILL_ACTIVE;
#else
    int status;
    return waitpid(childPid_, &status, WNOHANG) == 0;
#endif
}
