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

#include <mutex>
#include <string>
#include <string_view>
#include <iostream>

 /**
  * @brief Thread-safe logger with context prefix support.
  *        Currently logs to stdout; can be extended for file logging.
  */
class Logger {
public:
    /**
     * @brief Logs a message with a given prefix.
     * @param prefix Logical source (e.g. engine identifier).
     * @param message Log content (no newline required).
     */
    void log(std::string_view prefix, std::string_view message, bool isOutput) {
        std::scoped_lock lock(mutex_);
        std::cout << prefix << (isOutput ? " -> " : " <- ") << message << std::endl;
    }

    /**
     * @brief Returns the global logger instance.
     */
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

private:
    std::mutex mutex_;
};