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

#include "book-reader.h"

#include "../qapla-engine/book9/book.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>

namespace QaplaTester {

namespace {

using Book = QaplaBook9::Book<>;

/**
 * @brief The node array of a book of any payload, reduced to the two bytes of move and tree bits.
 */
std::vector<Book::Node> readNodes(std::ifstream& stream, uint32_t count, uint32_t nodeSize) {
    std::vector<Book::Node> nodes(count);
    std::vector<char> bytes(nodeSize);
    for (auto& node : nodes) {
        stream.read(bytes.data(), static_cast<std::streamsize>(nodeSize));
        if (!stream) {
            throw QaplaBook::BookFormatError("file ends inside the node array");
        }
        // Little endian, as every book file.
        node._bits = static_cast<uint16_t>(static_cast<uint8_t>(bytes[0])
            | (static_cast<uint8_t>(bytes[1]) << 8));
    }
    return nodes;
}

std::string toLan(const QaplaBook9::BookMove& move) {
    std::string lan = QaplaBasics::squareToString(move.from) + QaplaBasics::squareToString(move.to);
    if (move.promotion != QaplaBasics::NO_PIECE) {
        lan += QaplaBasics::pieceToPromoteChar(move.promotion);
    }
    return lan;
}

class LineCollector {
public:
    LineCollector(const Book& book, std::optional<size_t> maxLines, BookReaderResult& result)
        : book_(book), maxLines_(maxLines), result_(result) {}

    void collect(Book::NodeIndex node, const QaplaBook9::BookBoard& board) {
        for (auto child = book_.firstChild(node); child != Book::NO_NODE; child = book_.nextSibling(child)) {
            if (isFull()) {
                return;
            }
            if (book_.priority(child) == QaplaBook9::PRIORITY_NEVER) {
                ++result_.neverPlayed;
                continue;
            }
            const auto move = book_.move(child, board);
            line_.push_back(toLan(QaplaBook9::toKingTargetCastling(move, board)));
            if (book_.isLeaf(child)) {
                addLine();
            } else {
                auto next = board;
                next.doMove(move);
                collect(child, next);
            }
            line_.pop_back();
        }
    }

private:
    [[nodiscard]] bool isFull() const {
        return maxLines_ && result_.games.size() >= *maxLines_;
    }

    void addLine() {
        GameRecord game;
        for (const auto& lan : line_) {
            MoveRecord move;
            move.original = lan;
            game.addMove(move);
        }
        result_.games.push_back(std::move(game));
    }

    const Book& book_;
    std::optional<size_t> maxLines_;
    BookReaderResult& result_;
    std::vector<std::string> line_;
};

} // namespace

BookReaderResult readBookLines(const std::string& filePath, std::optional<size_t> maxLines) {
    BookReaderResult result;
    try {
        std::ifstream stream(filePath, std::ios::binary);
        if (!stream) {
            result.error = "cannot open " + filePath;
            return result;
        }
        QaplaBook::BookHeader header;
        const uint32_t count = QaplaBook::readBookHeader(stream, header);
        if (header.fileVersion != QaplaBook9::BOOK9_FILE_VERSION) {
            // Versions 1 to 3 are the books of Spike and the 11 bit books of Qapla.
            const bool elevenBit = header.fileVersion == "1" || header.fileVersion == "2"
                || header.fileVersion == "3";
            result.error = std::format("file version {}, a 9 bit book has {}{}", header.fileVersion,
                QaplaBook9::BOOK9_FILE_VERSION,
                elevenBit ? " - this is an 11 bit book, which is not supported" : "");
            return result;
        }
        // A file that is no book can still get through the header; its node count then promises
        // more than the file holds, and that is refused before anything is allocated.
        const auto available = std::filesystem::file_size(filePath) - static_cast<uintmax_t>(stream.tellg());
        if (header.nodeSize < 2 || static_cast<uintmax_t>(count) * header.nodeSize > available) {
            result.error = std::format("{} nodes of {} bytes do not fit into the file", count, header.nodeSize);
            return result;
        }
        Book book;
        book.fromPreorder(readNodes(stream, count, header.nodeSize));
        result.moves = book.size();
        LineCollector(book, maxLines, result).collect(Book::ROOT, QaplaBook9::BookBoard::startPosition());
    } catch (const std::exception& exc) {
        result.games.clear();
        result.error = exc.what();
    }
    return result;
}

} // namespace QaplaTester
