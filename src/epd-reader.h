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

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

/**
 * Represents the parsed contents of a single EPD line.
 */
struct EpdEntry {
    std::string fen; ///< Full FEN string (first 4 fields only)
    std::unordered_map<std::string, std::vector<std::string>> operations; ///< Opcode to operands
};

class EpdReader {
public:
    /**
     * Constructs an EpdReader and loads all entries from the specified file.
     *
     * @param filePath Path to the EPD file.
     * @throws std::runtime_error if the file cannot be read.
     */
    explicit EpdReader(const std::string& filePath);

    /**
     * Resets the reading position to the beginning.
     */
    void reset();

    /**
     * Returns the next EPD entry if available.
     *
     * @return std::optional containing the next EpdEntry, or std::nullopt if end reached.
     */
    std::optional<EpdEntry> next();

    /**
     * Returns all loaded EPD entries.
     *
     * @return const reference to the vector of all entries.
     */
    const std::vector<EpdEntry>& all() const;

	const std::string getFilePath() const {
		return filePath_;
	}

private:
    std::vector<EpdEntry> entries_;
    std::size_t currentIndex_;
    std::string filePath_;

    /**
     * Parses a single EPD line into FEN and opcode-operand pairs.
     *
     * @param line The EPD line as a string.
     * @return Parsed EpdEntry structure.
     * @throws std::runtime_error on invalid format.
     */
    EpdEntry parseEpdLine(const std::string& line);

    std::string extractFen(std::istringstream& stream);

    void parseOperations(const std::string& input, EpdEntry& result);

};
