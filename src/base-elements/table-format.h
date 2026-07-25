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

#include "qapla-json.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace QaplaTester {

/**
 * @brief Represents a single table cell value.
 */
struct TableCell {
    /**
     * @brief Cell payload supporting text and numeric values.
     */
    std::variant<std::string, std::int64_t, double> value;

    /**
     * @brief Creates an empty text cell.
     */
    TableCell() : value(std::string()) {}

    /**
     * @brief Creates a text cell from a string reference.
     * @param text Source text value.
     */
    TableCell(const std::string& text) : value(text) {}

    /**
     * @brief Creates a text cell by moving a string.
     * @param text Source text value.
     */
    TableCell(std::string&& text) : value(std::move(text)) {}

    /**
     * @brief Creates a text cell from a string view.
     * @param text Source text value.
     */
    TableCell(std::string_view text) : value(std::string(text)) {}

    /**
     * @brief Creates a text cell from a C-string.
     * @param text Source text value.
     */
    TableCell(const char* text) : value(std::string(text)) {}

    /**
     * @brief Creates an integer cell from a signed 64-bit value.
     * @param number Numeric value.
     */
    TableCell(std::int64_t number) : value(number) {}

    /**
     * @brief Creates an integer cell from an unsigned 64-bit value.
     * @param number Numeric value.
     */
    TableCell(std::uint64_t number) : value(static_cast<std::int64_t>(number)) {}

    /**
     * @brief Creates an integer cell from a signed integer value.
     * @param number Numeric value.
     */
    TableCell(int number) : value(static_cast<std::int64_t>(number)) {}

    /**
     * @brief Creates an integer cell from an unsigned integer value.
     * @param number Numeric value.
     */
    TableCell(unsigned int number) : value(static_cast<std::int64_t>(number)) {}

    /**
     * @brief Creates a floating-point cell from a double value.
     * @param number Numeric value.
     */
    TableCell(double number) : value(number) {}

    /**
     * @brief Creates a floating-point cell from a float value.
     * @param number Numeric value.
     */
    TableCell(float number) : value(static_cast<double>(number)) {}
};

/**
 * @brief Stores tabular data including headers, column widths, and rows.
 */
struct TableData {
    /**
     * @brief Optional column widths used for formatted rendering.
     */
    std::vector<size_t> columnWidths;

    /**
     * @brief Column header names.
     */
    std::vector<std::string> headers;

    /**
     * @brief Table body rows.
     */
    std::vector<std::vector<TableCell>> body;
};

/**
 * @brief Formats table data as text or JSON output.
 */
class TableFormat {
public:
    /**
     * @brief Formats table data into an aligned plain text table.
     * @param table Source table data.
     * @return Text representation of the table.
     */
    [[nodiscard]] static std::string toText(const TableData& table);

    /**
     * @brief Formats table data into a JSON payload tree.
     * @param tableName Logical name of the table in the JSON payload.
     * @param table Source table data.
     * @return JSON representation of the table.
     */
    [[nodiscard]] static Json::JsonValue toJsonValue(std::string_view tableName, const TableData& table);
};

} // namespace QaplaTester
