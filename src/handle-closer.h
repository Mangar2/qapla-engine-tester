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
#ifdef _WIN32

 /**
  * @brief Asynchronous and safe Windows handle closer.
  *
  * This class manages a dedicated background thread that closes Windows handles
  * using `CloseHandle`. It protects against hangs (e.g. after resume from hibernation)
  * by detecting unresponsive calls and restarting the worker thread if necessary.
  *
  * This header does **not** include `windows.h`. The `HANDLE` type is represented
  * as `void*` to avoid Windows header pollution. Internally, it is cast to `HANDLE`.
  *
  * Usage:
  * @code
  *     QaplaChess::HandleCloser::instance().close(someHandle);
  * @endcode
  */
class HandleCloser {
public:
    /**
     * @brief Returns the global singleton instance.
     *
     * The singleton instance is thread-safe and created on first use.
     *
     * @return Reference to the singleton HandleCloser instance.
     */
    static HandleCloser& instance() {
        static HandleCloser singleton;
        return singleton;
    }

    /**
     * @brief Submits a handle to be closed asynchronously.
     *
     * This function is non-blocking. The actual `CloseHandle` call is performed
     * in a dedicated background thread. If `CloseHandle` hangs beyond the timeout,
     * the worker thread is terminated and restarted.
     *
     * @param h A Windows HANDLE (cast as void*) to be closed.
     */
    void close(void* h);

    /**
     * @brief Waits for shutdown and cleans up resources.
     */
    void closeAllHandles();

    // Delete copy and move
    HandleCloser(const HandleCloser&) = delete;
    HandleCloser& operator=(const HandleCloser&) = delete;
    HandleCloser(HandleCloser&&) = delete;
    HandleCloser& operator=(HandleCloser&&) = delete;

private:
    HandleCloser(); // only accessible via instance()

	// Implementation is here to avoid including windows.h in the header
    class Impl;
    Impl* impl;
};

#endif // _WIN32