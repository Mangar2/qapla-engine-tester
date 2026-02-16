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
#include <string_view>
#include <variant>
#include <vector>

namespace QaplaTester {

struct TableCell {
    std::variant<std::string, std::int64_t, double> value;

    TableCell() : value(std::string()) {}
    TableCell(const std::string& text) : value(text) {}
    TableCell(std::string&& text) : value(std::move(text)) {}
    TableCell(std::string_view text) : value(std::string(text)) {}
    TableCell(const char* text) : value(std::string(text)) {}
    TableCell(std::int64_t number) : value(number) {}
    TableCell(std::uint64_t number) : value(static_cast<std::int64_t>(number)) {}
    TableCell(int number) : value(static_cast<std::int64_t>(number)) {}
    TableCell(unsigned int number) : value(static_cast<std::int64_t>(number)) {}
    TableCell(double number) : value(number) {}
    TableCell(float number) : value(static_cast<double>(number)) {}
};

struct TableData {
    std::vector<size_t> columnWidths;
    std::vector<std::string> headers;
    std::vector<std::vector<TableCell>> body;
};

class TableFormat {
public:
    [[nodiscard]] static std::string toText(const TableData& table);
    [[nodiscard]] static std::string toJson(std::string_view tableName, const TableData& table);
};

} // namespace QaplaTester
