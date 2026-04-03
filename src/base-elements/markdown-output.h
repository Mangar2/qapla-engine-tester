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

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace QaplaHelpers {

/**
 * @brief Builds Markdown output from structured content.
 */
class MarkdownOutput {
public:
    /**
     * @brief Adds a heading line.
     * @param text Heading text.
     * @param level Heading level in range [1, 6].
     */
    void addHeading(std::string_view text, int level = 1);

    /**
     * @brief Adds a plain paragraph.
     * @param text Paragraph text.
     */
    void addParagraph(std::string_view text);

    /**
     * @brief Adds a markdown table.
     * @param headers Column headers.
     * @param rows Row values.
     */
    void addTable(const std::vector<std::string>& headers,
        const std::vector<std::vector<std::string>>& rows);

    /**
     * @brief Adds a blank line.
     */
    void addBlankLine();

    /**
     * @brief Returns current markdown output.
     * @return Markdown text.
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Wraps text in markdown emphasis markers.
     * @param text Source text.
     * @return Emphasized markdown inline text.
     */
    [[nodiscard]] static std::string emphasis(std::string_view text);

private:
    [[nodiscard]] static std::string escapeTableCell(std::string_view text);
    [[nodiscard]] static int clampHeadingLevel(int level);

    std::string content_;
};

} // namespace QaplaHelpers
