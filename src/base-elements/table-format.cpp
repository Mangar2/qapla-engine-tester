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
#include "string-helper.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <sstream>
#include <type_traits>

namespace {

[[nodiscard]] bool isCpColumn(std::string_view header) {
    return QaplaHelpers::to_lowercase(std::string(header)).find("cp") != std::string::npos;
}

[[nodiscard]] std::string trimTrailingZeros(std::string value) {
    const auto dotPosition = value.find('.');
    if (dotPosition == std::string::npos) {
        return value;
    }

    while (!value.empty() && value.back() == '0') {
        value.pop_back();
    }
    if (!value.empty() && value.back() == '.') {
        value.pop_back();
    }
    if (value.empty()) {
        return "0";
    }
    return value;
}

[[nodiscard]] std::string formatDoubleForText(double value, std::string_view header) {
    if (isCpColumn(header)) {
        return std::format("{:.1f}", value);
    }
    return trimTrailingZeros(std::format("{:.4f}", value));
}

[[nodiscard]] std::string formatCellForText(const QaplaTester::TableCell& cell, std::string_view header) {
    return std::visit([header](const auto& value) -> std::string {
        using ValueType = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<ValueType, std::string>) {
            return value;
        } else if constexpr (std::is_same_v<ValueType, std::int64_t>) {
            return std::format("{}", value);
        } else {
            return formatDoubleForText(value, header);
        }
    }, cell.value);
}

[[nodiscard]] QaplaTester::Json::JsonValue formatCellForJson(const QaplaTester::TableCell& cell, std::string_view header) {
    return std::visit([header](const auto& value) -> QaplaTester::Json::JsonValue {
        using ValueType = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<ValueType, std::string>) {
            return value;
        } else if constexpr (std::is_same_v<ValueType, std::int64_t>) {
            return static_cast<double>(value);
        } else if (isCpColumn(header)) {
            return std::round(value * 10.0) / 10.0;
        } else {
            return value;
        }
    }, cell.value);
}

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
            const auto& header = columnIndex < table.headers.size() ? table.headers[columnIndex] : std::string_view();
            const auto cellText = formatCellForText(row[columnIndex], header);
            widths[columnIndex] = std::max(widths[columnIndex], cellText.size());
        }
    }

    for (auto& width : widths) {
        if (width == 0) {
            width = 1;
        }
    }

    return widths;
}

[[nodiscard]] std::string formatRow(const std::vector<QaplaTester::TableCell>& rowValues,
    const std::vector<std::string>& headers,
    const std::vector<size_t>& widths) {
    std::string line;
    for (size_t columnIndex = 0; columnIndex < widths.size(); ++columnIndex) {
        if (columnIndex > 0) {
            line += " | ";
        }
        const auto& header = columnIndex < headers.size() ? headers[columnIndex] : std::string_view();
        const auto cellValue = columnIndex < rowValues.size()
            ? formatCellForText(rowValues[columnIndex], header)
            : std::string();
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
        std::vector<TableCell> headerValues;
        headerValues.reserve(columnCount);
        for (size_t columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
            if (columnIndex < table.headers.size()) {
                headerValues.emplace_back(table.headers[columnIndex]);
            } else {
                headerValues.emplace_back("");
            }
        }
        stream << formatRow(headerValues, table.headers, widths) << "\n";
        stream << formatSeparator(widths);
        if (!table.body.empty()) {
            stream << "\n";
        }
    }

    for (size_t rowIndex = 0; rowIndex < table.body.size(); ++rowIndex) {
        auto rowValues = table.body[rowIndex];
        rowValues.resize(columnCount);
        stream << formatRow(rowValues, table.headers, widths);
        if (rowIndex + 1 < table.body.size()) {
            stream << "\n";
        }
    }

    return stream.str();
}

Json::JsonValue TableFormat::toJsonValue(std::string_view tableName, const TableData& table) {
    const auto columnCount = detectColumnCount(table);
    const auto widths = createEffectiveWidths(table, columnCount);

    auto payload = Json::JsonValue::object();
    payload["type"] = "table";
    payload["name"] = std::string(tableName);

    auto& columns = payload["columns"] = Json::JsonValue::array();
    for (size_t columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
        const auto headerValue = columnIndex < table.headers.size() ? table.headers[columnIndex] : std::string();
        auto& column = columns[columnIndex];
        column["header"] = headerValue;
        column["width"] = static_cast<double>(widths[columnIndex]);
    }

    auto& rows = payload["rows"] = Json::JsonValue::array();
    for (size_t rowIndex = 0; rowIndex < table.body.size(); ++rowIndex) {
        auto& rowValues = rows[rowIndex];
        for (size_t columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
            const auto& header = columnIndex < table.headers.size() ? table.headers[columnIndex] : std::string_view();
            const auto& cellValue = columnIndex < table.body[rowIndex].size()
                ? table.body[rowIndex][columnIndex]
                : TableCell("");
            rowValues[columnIndex] = formatCellForJson(cellValue, header);
        }
    }

    return payload;
}

} // namespace QaplaTester
