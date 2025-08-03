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

inline std::string to_lowercase(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
        [](char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

inline std::string to_alphanum(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (char ch : input) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result += ch;
        }
    }
    return result;
}

inline std::string trim(const std::string& line) {
	auto start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
	auto end = line.find_last_not_of(" \t\r\n");
	return line.substr(start, end - start + 1);
}

inline bool isInteger(const std::string& s) {
    int value;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    return ec == std::errc() && ptr == s.data() + s.size();
}

inline std::optional<std::string> parseSection(const std::string& line) {
    if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
        return trim(line.substr(1, line.size() - 2));
    }
    return std::nullopt;
}

inline std::optional<std::string> readSectionHeader(std::istream& in) {
    std::string line;
    while (in && std::getline(in, line)) {
        std::string trimmedLine = trim(line);
        if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';') continue;
        auto section = parseSection(trimmedLine);
		if (section) {
			return *section;
		}
    }
    return std::nullopt;
}

inline std::optional<std::pair<std::string, std::string>> parseKeyValue(const std::string& line) {
    auto eq = line.find('=');
    if (eq == std::string::npos) return std::nullopt;
    std::string key = trim(line.substr(0, eq));
    std::string value = trim(line.substr(eq + 1));
    if (key.empty()) return std::nullopt;
    return std::make_pair(key, value);
}

inline std::string formatMs(uint64_t ms) {
    std::ostringstream oss;
    uint64_t minutes = ms / 60000;
    uint64_t seconds = (ms % 60000) / 1000;
    uint64_t millis = ms % 1000;
    oss << minutes << ":" 
        << std::right
        << std::setw(2) << std::setfill('0') << seconds << "." 
        << std::setw(3) << std::setfill('0') << millis;
    return oss.str();
}

inline size_t levenshteinDistance(const std::string& a, const std::string& b) {
    const size_t m = a.size(), n = b.size();
    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));
    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;
    for (size_t i = 1; i <= m; ++i)
        for (size_t j = 1; j <= n; ++j)
            dp[i][j] = std::min({ dp[i - 1][j - 1] + (a[i - 1] != b[j - 1]), dp[i - 1][j] + 1, dp[i][j - 1] + 1 });
    return dp[m][n];
};
