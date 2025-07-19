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

#ifdef _WIN32

#include "handle-closer.h"
#include "logger.h"
#include <windows.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

class HandleCloser::Impl {
public:
    Impl() {
        startControlThread();
        workerThread = CreateThread(nullptr, 0, workerEntry, this, 0, nullptr);
    }

    ~Impl() {
        stopWorker = true;
        workCond.notify_one();

        if (controlThread.joinable())
            controlThread.join();

        if (workerThread) {
            DWORD wait = WaitForSingleObject(workerThread, 2000);
            if (wait != WAIT_OBJECT_0) {
                Logger::testLogger().log("HandleCloser: worker did not exit cleanly.", TraceLevel::error);
                TerminateThread(workerThread, 1);
            }
            CloseHandle(workerThread);
        }
    }

    void restartWorker() {
        {
            std::lock_guard lock(queueMutex);
            if (workerThread) {
                TerminateThread(workerThread, 1);
                CloseHandle(workerThread);
            }
        }

        workerThread = CreateThread(nullptr, 0, workerEntry, this, 0, nullptr);
        if (!workerThread) {
            Logger::testLogger().log("HandleCloser: restartWorker failed to create thread.", TraceLevel::error);
        }
    }


    void close(void* h) {
        HANDLE handle = static_cast<HANDLE>(h);
        if (!handle || handle == INVALID_HANDLE_VALUE)
            return;

        {
            std::lock_guard lock(queueMutex);
            handleQueue.push(handle);
        }
        workCond.notify_one();
    }

    void startCountdown() {
        {
			if (verbose) std::cout << "HandleCloser: Starting countdown for handle closure." << std::endl;
            std::lock_guard lock(timeoutMutex); // signal: begin waiting
            abortTimeout = false;
        }
        if (verbose) std::cout << "HandleCloser: Countdown started, waiting for control thread." << std::endl;
        timeoutCond.notify_one();
    }

    void abortCountdown() {
        {
            if (verbose) std::cout << "HandleCloser: Aborting countdown for handle closure." << std::endl;
            std::lock_guard lock(timeoutMutex); // signal: begin waiting
            abortTimeout = true;
        }
        if (verbose) std::cout << "HandleCloser: Countdown aborted, notifying control thread." << std::endl;
        timeoutCond.notify_one();
    }

    void startControlThread() {
        timeoutMutex.lock();
        controlThread = std::thread([this] {
			if (verbose) std::cout << "HandleCloser: Control thread started." << std::endl;
            std::unique_lock timeoutLock(timeoutMutex, std::adopt_lock);
            while (!stopControl) {
                if (verbose) std::cout << "HandleCloser: Waiting for control timeout or stop signal." << std::endl;
				if (abortTimeout == false) std::cout << "HandleCloser: Waiting for control timeout with abort = false." << std::endl;
                timeoutCond.wait(timeoutLock, [&] { return stopControl || !abortTimeout; });

                if (stopControl) break;
                if (verbose) std::cout << "HandleCloser: Control timeout reached." << std::endl;
                timeoutCond.wait_for(timeoutLock, std::chrono::seconds(200), [&] { return stopControl || abortTimeout; });

				if (abortTimeout == false) std::cout << "Timeout with abort = false" << std::endl;
                if (verbose) std::cout << "HandleCloser: Control timeout check." << std::endl;
                if (!abortTimeout && !stopControl) {
                    if (verbose) std::cout << "HandleCloser: Control timeout - closing handles." << std::endl;
                    timeoutLock.unlock();
                    Logger::testLogger().log("HandleCloser: Control timeout - restarting worker.", TraceLevel::error);
                    restartWorker();
                    if (verbose) std::cout << "HandleCloser: Worker restarted." << std::endl;
                    timeoutLock.lock();
                }

                abortTimeout = true;
            }
            });
    }

private:
    std::mutex queueMutex;
    std::condition_variable workCond;
    std::queue<HANDLE> handleQueue;
    std::atomic<bool> stopWorker{ false };
	std::atomic<bool> stopControl{ false };
    HANDLE workerThread;

    std::thread controlThread;
    std::condition_variable timeoutCond;
    std::mutex timeoutMutex;   
    std::atomic<bool> abortTimeout{ true };
    bool verbose = false;

    static DWORD WINAPI workerEntry(LPVOID param) {
        return static_cast<Impl*>(param)->workerLoop();
    }

    DWORD workerLoop() {
        if (verbose) std::cout << "HandleCloser: Worker thread started." << std::endl;
        std::unique_lock queueLock(queueMutex);
        while (true) {

            if (handleQueue.empty()) {
                if (stopWorker) {
                    stopControl = true;
                    timeoutCond.notify_one();
                    break;
                }
                workCond.wait(queueLock, [&] { return stopWorker || !handleQueue.empty(); });
                continue;
            }

            while (!handleQueue.empty()) {
                HANDLE h = handleQueue.front();
                handleQueue.pop();
                queueLock.unlock();
                startCountdown();

                ::CloseHandle(h);

                abortCountdown();
				// Wait until ControlThread is waiting again
                std::lock_guard sync(timeoutMutex);
                queueLock.lock();
            }
        }
        return 0;
    }
};

// Public forwarding
HandleCloser::HandleCloser() : impl(new Impl()) {}
void HandleCloser::closeAllHandles() { delete impl; impl = 0; }
void HandleCloser::close(void* h) { impl->close(h); }

#endif