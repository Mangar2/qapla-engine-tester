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
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include "markdown-output.h"

#include <algorithm>

namespace QaplaHelpers {

void MarkdownOutput::addHeading(std::string_view text, int level) {
    const auto headingLevel = clampHeadingLevel(level);
    content_.append(static_cast<size_t>(headingLevel), '#');
    content_ += " ";
    content_ += std::string(text);
    addBlankLine();
}

void MarkdownOutput::addParagraph(std::string_view text) {
    content_ += std::string(text);
    addBlankLine();
}

void MarkdownOutput::addTable(const std::vector<std::string>& headers,
    const std::vector<std::vector<std::string>>& rows) {
    if (headers.empty()) {
        return;
    }

    content_ += "|";
    for (const auto& header : headers) {
        content_ += " ";
        content_ += escapeTableCell(header);
        content_ += " |";
    }
    content_ += "\n";

    content_ += "|";
    for (size_t headerIndex = 0; headerIndex < headers.size(); ++headerIndex) {
        content_ += " --- |";
    }
    content_ += "\n";

    for (const auto& row : rows) {
        content_ += "|";
        for (size_t cellIndex = 0; cellIndex < headers.size(); ++cellIndex) {
            content_ += " ";
            if (cellIndex < row.size()) {
                content_ += escapeTableCell(row[cellIndex]);
            }
            content_ += " |";
        }
        content_ += "\n";
    }

    addBlankLine();
}

void MarkdownOutput::addBlankLine() {
    content_ += "\n";
}

std::string MarkdownOutput::toString() const {
    return content_;
}

std::string MarkdownOutput::emphasis(std::string_view text) {
    return "*" + std::string(text) + "*";
}

std::string MarkdownOutput::escapeTableCell(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const auto character : text) {
        if (character == '|') {
            result += "\\|";
            continue;
        }
        if (character == '\n') {
            result += ' ';
            continue;
        }
        result += character;
    }
    return result;
}

int MarkdownOutput::clampHeadingLevel(int level) {
    return std::clamp(level, 1, 6);
}

} // namespace QaplaHelpers
