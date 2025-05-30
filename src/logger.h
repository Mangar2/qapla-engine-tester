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
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

enum class TraceLevel : int {
    error,
    commands,
    handshake,
    warning,
    info,
    none
};

/**
 * @brief Thread-safe logger with optional file output and trace filtering.
 */
class Logger {
public:
    Logger() : traceLevelThreshold_(TraceLevel::error) {
    }

    ~Logger() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }

    /**
	 * @brief Logs a message with a given prefix. It logs all messages to the file and some based on trace level to stdout.
     * @param prefix Logical source (e.g. engine identifier).
     * @param message Log content (no newline required).
     * @param isOutput true if engine output, false if input.
     * @param level Trace level of this message for logging t cout (default: info).
     */
    void log(std::string_view prefix, std::string_view message, bool isOutput, TraceLevel level = TraceLevel::info) {

        std::scoped_lock lock(mutex_);
        if (fileStream_.is_open()) {
            fileStream_ << prefix << (isOutput ? " -> " : " <- ") << message << std::endl;
        }
        
        if (level > traceLevelThreshold_) return;
		std::cout << prefix << (isOutput ? " -> " : " <- ") << message << std::endl;

    }

    /**
     * @brief Logs a message 
     * @param message Log content (no newline required).
     * @param level Trace level of this message for logging t cout (default: info).
     */
    void log(std::string_view message, TraceLevel level = TraceLevel::commands) {

        std::scoped_lock lock(mutex_);
        if (fileStream_.is_open()) {
            fileStream_ << message << std::endl;
        }

        if (level > traceLevelThreshold_) return;
        std::cout << message << std::endl;
    }

    /**
     * @brief Sets the output log file.
     * @param filename Path to target file.
     */
    void setLogFile(const std::string& basename) {
        std::scoped_lock lock(mutex_);
        filename_ = generateTimestampedFilename(basename);
        fileStream_.close();
        fileStream_.open(filename_, std::ios::app);
    }

	/**
	 * @brief Returns the current log file name.
	 * @return The name of the log file.
	 */
	std::string getFilename() const {
		return filename_;
	}

    /**
     * @brief Sets the minimum trace level to log.
     * @param level TraceLevel threshold.
     */
    void setTraceLevel(TraceLevel level) {
        traceLevelThreshold_ = level;
    }

    /**
     * @brief Returns the global logger instance.
     */
    static Logger& engineLogger() {
        static Logger instance;
        return instance;
    }

    static Logger& testLogger() {
        static Logger instance;
        return instance;
    }

private:

    std::string generateTimestampedFilename(const std::string& baseName) {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto now_time_t = system_clock::to_time_t(now);
        auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::tm local_tm;
#ifdef _WIN32
        localtime_s(&local_tm, &now_time_t);
#else
        localtime_r(&now_time_t, &local_tm);
#endif

        std::ostringstream oss;
        oss << baseName << '-'
            << std::put_time(&local_tm, "%Y-%m-%d_%H-%M-%S")
            << '.' << std::setw(3) << std::setfill('0') << now_ms.count()
            << ".log";
        return oss.str();
    }

    std::mutex mutex_;
    std::ofstream fileStream_;
    TraceLevel traceLevelThreshold_;
	std::string filename_;
};
