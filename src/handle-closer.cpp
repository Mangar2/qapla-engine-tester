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
#include "timer.h"
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
        startWorker();
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

    void stopWorkerThread() {
        std::lock_guard lock(queueMutex);
        if (workerThread) {
            TerminateThread(workerThread, 1);
            CloseHandle(workerThread);
        }
    }

    void startWorker() {

        workerThread = CreateThread(nullptr, 0, workerEntry, this, 0, nullptr);

        if (!workerThread) {
            Logger::testLogger().log("HandleCloser: startWorker failed to create thread.", TraceLevel::error);
        }
    }


    void close(void* h) {
        std::cout << "HandleCloser: Request to close handle: " << h << std::endl;
        HANDLE handle = static_cast<HANDLE>(h);
        if (!handle || handle == INVALID_HANDLE_VALUE) {
            std::cout << "HandleCloser: Invalid handle provided, skipping closure." << std::endl;
            return;
        }

        {
            std::lock_guard lock(queueMutex);
            handleQueue.push(handle);
        }
        workCond.notify_one();
    }

    void startCountdown() {
        {
			if (verbose) std::cout << "before abortTimeout = false" << std::endl;
            std::lock_guard lock(timeoutMutex); // signal: begin waiting
            abortTimeout = false;
        }
        if (verbose) std::cout << "Notify to wakeup timeout thread" << std::endl;
        timeoutCond.notify_one();
    }

    void abortCountdown() {
        {
            if (verbose) std::cout << "before abortTimeout = true" << std::endl;
            std::lock_guard lock(timeoutMutex); // signal: begin waiting
            abortTimeout = true;
        }
        if (verbose) std::cout << "notify timeout wait to stop waiting" << std::endl;
        timeoutCond.notify_one();
    }

    void startControlThread() {
        // The timeout mutex must be locked before the worker thread starts.
        timeoutMutex.lock();
        static int threadNo = 0;
        threadNo++;
        std::cout << "RUNNING THREAD NO " << threadNo << std::endl;
        controlThread = std::thread([this] {
			if (verbose) std::cout << "HandleCloser: Control thread started." << std::endl;
            // Waiting takes over the lock and releases ist. We needed to adopt the lock as it is created by another thread
            std::unique_lock timeoutLock(timeoutMutex, std::adopt_lock);
            while (!stopControl) {
                if (verbose) std::cout << "Waiting for next timeout task" << std::endl;
				
                // Waits until the worker starts closing a handle
                timeoutCond.wait(timeoutLock, [&] { return stopControl || !abortTimeout; });

                if (stopControl) break;
                
                if (verbose) std::cout << "Before timeout wait." << stopControl << " " << abortTimeout << std::endl;
                // Timeout starting exactly before close handle is called
                bool aborted = timeoutCond.wait_for(timeoutLock, std::chrono::seconds(30), [&] { return stopControl || abortTimeout; });
                if (verbose) std::cout << "After timeout wait stopControl:" << stopControl 
                    << " abortTimeout: " << abortTimeout << " aborted: " << aborted << std::endl;

                if (!abortTimeout && !stopControl) {
                    if (verbose) std::cout << "ERROR: restarting thread" << std::endl;
                    timeoutLock.unlock();
                    Logger::testLogger().log("System was not able to close a windows handle, the thread was restarted. "
                        + std::to_string(handleQueue.size()) + " Handles to close"
                        , TraceLevel::error);
                    stopWorkerThread();
                    timeoutLock.lock();
                    // timeoutLock must be locked when starting the new Worker
                    startWorker();
                    if (verbose) std::cout << "ERROR: thread restarted" << std::endl;
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
    std::atomic<bool> abortTimeout{ true };
    HANDLE workerThread;

    std::thread controlThread;
    std::condition_variable timeoutCond;
    std::mutex timeoutMutex;   
    bool verbose = true;

    static DWORD WINAPI workerEntry(LPVOID param) {
        return static_cast<Impl*>(param)->workerLoop();
    }

    DWORD workerLoop() {
        if (verbose) std::cout << "Worker thread started." << std::endl;
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
            Timer timer;
            while (!handleQueue.empty()) {
                HANDLE h = handleQueue.front();
                handleQueue.pop();
                queueLock.unlock();
                if (verbose) std::cout << "Starting to close handle" << std::endl;
                startCountdown();
                ::CloseHandle(h);
                abortCountdown();
				// Wait until ControlThread is waiting again
                
                queueLock.lock();
                if (verbose) std::cout << "Waiting for timeoutMutex" << std::endl;
                std::unique_lock timeoutLock(timeoutMutex);
                if (verbose) std::cout << "Waiting for timeoutMutex done" << std::endl;
                //workCond.wait(timeoutLock, [&] { return stopWorker || abortTimeout; });
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