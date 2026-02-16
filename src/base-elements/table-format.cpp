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

#include "table-format.h"
#include "json-helper.h"

#include <algorithm>
#include <format>
#include <sstream>

namespace {

[[nodiscard]] size_t detectColumnCount(const QaplaTester::TableData& table) {
    auto columnCount = std::max(table.columnWidths.size(), table.headers.size());
    for (const auto& row : table.body) {
        columnCount = std::max(columnCount, row.size());
    }
    return columnCount;
}

[[nodiscard]] std::vector<size_t> createEffectiveWidths(const QaplaTester::TableData& table, size_t columnCount) {
    std::vector<size_t> widths(columnCount, 0);
    for (size_t columnIndex = 0; columnIndex < table.columnWidths.size() && columnIndex < columnCount; ++columnIndex) {
        widths[columnIndex] = table.columnWidths[columnIndex];
    }

    for (size_t columnIndex = 0; columnIndex < table.headers.size() && columnIndex < columnCount; ++columnIndex) {
        widths[columnIndex] = std::max(widths[columnIndex], table.headers[columnIndex].size());
    }

    for (const auto& row : table.body) {
        for (size_t columnIndex = 0; columnIndex < row.size() && columnIndex < columnCount; ++columnIndex) {
            widths[columnIndex] = std::max(widths[columnIndex], row[columnIndex].size());
        }
    }

    for (auto& width : widths) {
        if (width == 0) {
            width = 1;
        }
    }

    return widths;
}

[[nodiscard]] std::string formatRow(const std::vector<std::string>& rowValues, const std::vector<size_t>& widths) {
    std::string line;
    for (size_t columnIndex = 0; columnIndex < widths.size(); ++columnIndex) {
        if (columnIndex > 0) {
            line += " | ";
        }
        const auto& cellValue = columnIndex < rowValues.size() ? rowValues[columnIndex] : std::string();
        line += std::format("{:<{}}", cellValue, widths[columnIndex]);
    }
    return line;
}

[[nodiscard]] std::string formatSeparator(const std::vector<size_t>& widths) {
    std::string separator;
    for (size_t columnIndex = 0; columnIndex < widths.size(); ++columnIndex) {
        if (columnIndex > 0) {
            separator += "-+-";
        }
        separator.append(widths[columnIndex], '-');
    }
    return separator;
}

} // namespace

namespace QaplaTester {

std::string TableFormat::toText(const TableData& table) {
    const auto columnCount = detectColumnCount(table);
    if (columnCount == 0) {
        return "<empty table>";
    }

    const auto widths = createEffectiveWidths(table, columnCount);
    std::ostringstream stream;

    if (!table.headers.empty()) {
        auto headerValues = table.headers;
        headerValues.resize(columnCount);
        stream << formatRow(headerValues, widths) << "\n";
        stream << formatSeparator(widths);
        if (!table.body.empty()) {
            stream << "\n";
        }
    }

    for (size_t rowIndex = 0; rowIndex < table.body.size(); ++rowIndex) {
        auto rowValues = table.body[rowIndex];
        rowValues.resize(columnCount);
        stream << formatRow(rowValues, widths);
        if (rowIndex + 1 < table.body.size()) {
            stream << "\n";
        }
    }

    return stream.str();
}

std::string TableFormat::toJson(std::string_view tableName, const TableData& table) {
    const auto columnCount = detectColumnCount(table);
    const auto widths = createEffectiveWidths(table, columnCount);

    Mcp::JsonValue::Array columns;
    for (size_t columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
        const auto headerValue = columnIndex < table.headers.size() ? table.headers[columnIndex] : std::string();
        Mcp::JsonValue::Object columnObject;
        columnObject["header"] = Mcp::JsonHelper::makeString(headerValue);
        columnObject["width"] = Mcp::JsonHelper::makeNumber(static_cast<double>(widths[columnIndex]));
        columns.push_back(Mcp::JsonHelper::makeObject(std::move(columnObject)));
    }

    Mcp::JsonValue::Array rows;
    for (size_t rowIndex = 0; rowIndex < table.body.size(); ++rowIndex) {
        Mcp::JsonValue::Array rowValues;
        for (size_t columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
            const auto& cellValue = columnIndex < table.body[rowIndex].size() ? table.body[rowIndex][columnIndex] : std::string();
            rowValues.push_back(Mcp::JsonHelper::makeString(cellValue));
        }
        rows.push_back(Mcp::JsonHelper::makeArray(std::move(rowValues)));
    }

    Mcp::JsonValue::Object payloadObject;
    payloadObject["type"] = Mcp::JsonHelper::makeString("table");
    payloadObject["name"] = Mcp::JsonHelper::makeString(tableName);
    payloadObject["columns"] = Mcp::JsonHelper::makeArray(std::move(columns));
    payloadObject["rows"] = Mcp::JsonHelper::makeArray(std::move(rows));

    const auto payload = Mcp::JsonHelper::makeObject(std::move(payloadObject));
    return Mcp::JsonHelper::serialize(payload);
}

} // namespace QaplaTester
