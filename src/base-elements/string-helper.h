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

#include <cstdint>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <vector>
#include <format>
#include <chrono>

namespace QaplaHelpers {

    /**
     * @brief Converts a string to lowercase.
     * @param input The input string.
     * @return The lowercase version of the input.
     */
    inline std::string to_lowercase(const std::string& input) {
        std::string result = input;
        std::ranges::transform(result, result.begin(),
            [](char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    /**
     * @brief Extracts alphanumeric characters from a string.
     * @param input The input string.
     * @return A string containing only alphanumeric characters.
     */
    inline std::string to_alphanum(const std::string& input) {
        std::string result;
        result.reserve(input.size());
        for (char ch : input) {
            if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
                result += ch;
            }
        }
        return result;
    }

    /**
     * @brief Trims whitespace from both ends of a string.
     * @param line The input string.
     * @return The trimmed string.
     */
    inline std::string trim(const std::string& line) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return {};
        }
        auto end = line.find_last_not_of(" \t\r\n");
        return line.substr(start, end - start + 1);
    }

    /**
     * @brief Checks if a string represents a valid integer.
     * @param s The string to check.
     * @return True if the string is a valid integer, false otherwise.
     */
    inline bool isInteger(const std::string& s) {
        int value;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        return ec == std::errc() && ptr == s.data() + s.size();
    }

    /**
     * @brief Converts a string view to an optional signed integer of any size.
     * @tparam T The signed integer type (int8_t, int16_t, int32_t, int64_t, int).
     * @param s The string view to convert.
     * @return Optional T if conversion succeeds, nullopt otherwise.
     */
    template<typename T>
    inline std::optional<T> to_signed_int(std::string_view s) {
        static_assert(std::is_integral_v<T> && std::is_signed_v<T>, "T must be a signed integral type");
        T value;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec == std::errc() && ptr == s.data() + s.size()) {
            return value;
        }
        return std::nullopt;
    }

    /**
     * @brief Converts a string view to an optional integer.
     * @param s The string view to convert.
     * @return Optional integer if conversion succeeds, nullopt otherwise.
     */
    inline auto to_int = [](std::string_view s) -> std::optional<int> {
        return to_signed_int<int>(s);
    };

    /**
     * @brief Checks if a string represents a valid unsigned integer.
     * @param s The string to check.
     * @return True if the string is a valid unsigned integer, false otherwise.
     */
    inline bool isUnsignedInteger(const std::string& s) {
        auto trimmed = trim(s);
        if (trimmed.empty()) {
            return false;
        }
        if (trimmed[0] == '-') {
            return false;
        }

        int value;
        auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
        return ec == std::errc() && ptr == trimmed.data() + trimmed.size();
    }

    /**
     * @brief Converts a string view to an optional unsigned integer of any size.
     * @tparam T The unsigned integer type (uint8_t, uint16_t, uint32_t, uint64_t, unsigned int).
     * @param s The string view to convert.
     * @return Optional T if conversion succeeds, nullopt otherwise.
     */
    template<typename T>
    inline std::optional<T> to_unsigned_int(std::string_view s) {
        static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "T must be an unsigned integral type");
        auto trimmed = trim(std::string(s));
        if (trimmed.empty() || trimmed[0] == '-') {
            return std::nullopt;
        }

        T value;
        auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
        if (ec == std::errc() && ptr == trimmed.data() + trimmed.size()) {
            return value;
        }
        return std::nullopt;
    }

    /**
     * @brief Converts a string view to an optional uint32_t.
     * @param s The string view to convert.
     * @return Optional uint32_t if conversion succeeds, nullopt otherwise.
     */
    inline auto to_uint32 = [](std::string_view s) -> std::optional<uint32_t> {
        return to_unsigned_int<uint32_t>(s);
    };

    /**
     * @brief Converts a string view to an optional double.
     * @param s The string view to convert.
     * @return Optional double if conversion succeeds, nullopt otherwise.
     */
    inline auto to_double = [](std::string_view s) -> std::optional<double> {
        try {
            double value = std::stod(std::string(s));
            return value;
        } catch (...) {
            return std::nullopt;
        }
    };

    /**
     * @brief Converts a string view to an optional float.
     * @param s The string view to convert.
     * @return Optional float if conversion succeeds, nullopt otherwise.
     */
    inline auto to_float = [](std::string_view s) -> std::optional<float> {
        try {
            float value = std::stof(std::string(s));
            return value;
        } catch (...) {
            return std::nullopt;
        }
    };

    /**
     * @brief Parses a section header from a line.
     * @param line The line to parse.
     * @return Optional section name if valid, nullopt otherwise.
     */
    inline std::optional<std::string> parseSection(const std::string& line) {
        if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
            return trim(line.substr(1, line.size() - 2));
        }
        return std::nullopt;
    }

    /**
     * @brief Reads the next section header from an input stream.
     * @param in The input stream.
     * @return Optional section name if found, nullopt otherwise.
     */
    inline std::optional<std::string> readSectionHeader(std::istream& in) {
        std::string line;
        while (in && std::getline(in, line)) {
            std::string trimmedLine = trim(line);
            if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';') {
                continue;
            }
            auto section = parseSection(trimmedLine);
            if (section) {
                return *section;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Parses a key-value pair from a line.
     * @param line The line to parse.
     * @return Optional pair of key and value if valid, nullopt otherwise.
     */
    inline std::optional<std::pair<std::string, std::string>> parseKeyValue(const std::string& line) {
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            return std::nullopt;
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key.empty()) {
            return std::nullopt;
        }
        return std::make_pair(key, value);
    }

    /**
     * @brief Formats a time_point into a time string (HH:MM:SS.mmm).
     * @param timestamp The time_point to format.
     * @return Formatted time string.
     */
    inline std::string formatTimeOfDay(const std::chrono::system_clock::time_point& timestamp) {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        
        std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &time_t);
    #else
        localtime_r(&time_t, &tm);
    #endif
        
        return std::format("{:02d}:{:02d}:{:02d}.{:03d}", 
            tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
    }

    /**
     * @brief Formats milliseconds into a time string.
     * @param ms The milliseconds to format.
     * @param mdigits The number of decimal digits for seconds (default 3).
     * @return Formatted time string.
     */
    inline std::string formatMs(uint64_t ms, uint32_t mdigits = 3) {
        std::ostringstream oss;
        uint64_t seconds = ms / 1000;
        uint64_t minutes = seconds / 60;
        uint64_t hours = minutes / 60;
        double dSeconds = static_cast<double>(ms % 60000) / 1000.0;
        if (hours > 0) {
            oss << hours << ":";
        }
        oss << std::right 
            << std::setfill('0') << std::setw(2) << (minutes % 60) << ":"
            << std::fixed << std::setprecision(mdigits);
        if (mdigits == 0) {
            oss << std::setw(2) << std::setfill('0') << seconds % 60;
        } else {
            oss << std::setw(mdigits + 3) << std::setfill('0')
                << dSeconds;
        }
        return oss.str();
    }

    /**
     * @brief Calculates the Levenshtein distance between two strings.
     * @param a The first string.
     * @param b The second string.
     * @return The edit distance.
     */
    inline size_t levenshteinDistance(const std::string& a, const std::string& b) {
        const size_t m = a.size();
        const size_t n = b.size();
        std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));
        for (size_t i = 0; i <= m; ++i) {
            dp[i][0] = i;
        }
        for (size_t j = 0; j <= n; ++j) {
            dp[0][j] = j;
        }
        for (size_t i = 1; i <= m; ++i) {
            for (size_t j = 1; j <= n; ++j) {
                dp[i][j] = std::min({ dp[i - 1][j - 1] + static_cast<size_t>(a[i - 1] != b[j - 1]), dp[i - 1][j] + 1, dp[i][j - 1] + 1 });
            }
        }
        return dp[m][n];
    };

    /**
     * @brief Reads the next token from a string view.
     * @param text Source text.
     * @param position Current offset; advanced to the end of the returned token.
     * @param delimiters Characters that delimit tokens. Defaults to single space.
     * @return Next token or empty string_view if no further token exists.
     */
    inline std::string_view readNextToken(
        std::string_view text,
        size_t& position,
        std::string_view delimiters = " ") {
        const auto tokenStart = text.find_first_not_of(delimiters, position);
        if (tokenStart == std::string_view::npos) {
            position = text.size();
            return {};
        }
        const auto tokenEnd = text.find_first_of(delimiters, tokenStart);
        if (tokenEnd == std::string_view::npos) {
            position = text.size();
            return text.substr(tokenStart);
        }
        position = tokenEnd;
        return text.substr(tokenStart, tokenEnd - tokenStart);
    }

    /**
     * @brief Splits a string by a delimiter character.
     * @param str The string to split.
     * @param delimiter The delimiter character.
     * @return Vector of substrings.
     */
    inline std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        if (str.empty()) {
            return result;
        }
        
        size_t start = 0;
        size_t end = str.find(delimiter);
        
        while (end != std::string::npos) {
            result.push_back(str.substr(start, end - start));
            start = end + 1;
            end = str.find(delimiter, start);
        }
        
        result.push_back(str.substr(start));
        return result;
    }

    /**
     * @brief Escapes a delimiter character in a string.
     * @param str The string to escape.
     * @param delimiter The delimiter to escape.
     * @return Escaped string.
     */
    inline std::string escapeDelimiter(const std::string& str, char delimiter) {
        std::string result;
        result.reserve(str.size());
        
        for (char ch : str) {
            if (ch == delimiter || ch == '\\') {
                result += '\\';
            }
            result += ch;
        }
        
        return result;
    }

    /**
     * @brief Unescapes a delimiter character in a string.
     * @param str The string to unescape.
     * @return Unescaped string.
     */
    inline std::string unescapeDelimiter(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        
        bool escaped = false;
        for (char ch : str) {
            if (escaped) {
                result += ch;
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else {
                result += ch;
            }
        }
        
        return result;
    }

    /**
     * @brief Splits a string by delimiter with proper unescaping.
     * @param str The string to split.
     * @param delimiter The delimiter character.
     * @return Vector of unescaped substrings.
     */
    inline std::vector<std::string> splitWithUnescape(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        if (str.empty()) {
            return result;
        }
        
        std::string current;
        bool escaped = false;
        
        for (char ch : str) {
            if (escaped) {
                current += ch;
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == delimiter) {
                result.push_back(current);
                current.clear();
            } else {
                current += ch;
            }
        }
        
        result.push_back(current);
        return result;
    }

    /**
     * @brief Converts ASCII string to wide string.
     * @param str The input ASCII string.
     * @return Wide string.
     * @note Only works correctly for ASCII characters (0-127). UTF-8 encoded strings will be corrupted.
     */
    inline std::wstring ascii_to_wstring(const std::string& str) {
        return { str.begin(), str.end() };
    }

    /**
     * @brief Parses a duration string in formats "S", "M:S", or "H:M:S".
     * @param s The string to parse.
     * @return Optional duration in seconds as double.
     */
    inline std::optional<double> parseDuration(std::string_view s) {
        auto parts = split(std::string(s), ':');
        if (parts.empty() || parts.size() > 3) {
            return std::nullopt;
        }

        double seconds = 0.0;
        
        if (parts.size() == 3) {
             // H:M:S
            auto h = to_unsigned_int<uint32_t>(parts[0]);
            auto m = to_unsigned_int<uint32_t>(parts[1]);
            auto sec = to_double(parts[2]);
            
            if (!h || !m || !sec) {
                return std::nullopt;
            }
            
            seconds += *h * 3600.0;
            seconds += *m * 60.0;
            seconds += *sec;
        } else if (parts.size() == 2) {
            // M:S
            auto m = to_unsigned_int<uint32_t>(parts[0]);
            auto sec = to_double(parts[1]);

            if (!m || !sec) {
                return std::nullopt;
            }

            seconds += *m * 60.0;
            seconds += *sec;
        } else {
            // S
            auto sec = to_double(parts[0]);
            if (!sec) {
                return std::nullopt;
            }
            seconds += *sec;
        }
        
        return seconds;
    }

} // namespace QaplaHelpers
