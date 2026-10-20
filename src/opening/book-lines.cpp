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

#include "book-lines.h"

#include "../qapla-engine/book9/book.h"

#include <filesystem>
#include <format>
#include <fstream>

namespace QaplaTester {

namespace {

using Node = QaplaBook9::PackedBookNode<QaplaBook::EmptyPayload>;

Node nodeOf(uint16_t bits) {
    Node node;
    node._bits = bits;
    return node;
}

std::string toLan(const QaplaBook9::BookMove& move) {
    std::string lan = QaplaBasics::squareToString(move.from) + QaplaBasics::squareToString(move.to);
    if (move.promotion != QaplaBasics::NO_PIECE) {
        lan += QaplaBasics::pieceToPromoteChar(move.promotion);
    }
    return lan;
}

} // namespace

BookLines BookLines::read(const std::string& filePath, std::optional<size_t> maxLines) {
    std::ifstream stream(filePath, std::ios::binary);
    if (!stream) {
        throw QaplaBook::BookFormatError("cannot open " + filePath);
    }
    QaplaBook::BookHeader header;
    const uint32_t count = QaplaBook::readBookHeader(stream, header);
    if (header.fileVersion != QaplaBook9::BOOK9_FILE_VERSION) {
        // Versions 1 to 3 are the books of Spike and the 11 bit books of Qapla.
        const bool elevenBit = header.fileVersion == "1" || header.fileVersion == "2"
            || header.fileVersion == "3";
        throw QaplaBook::BookFormatError(std::format("file version {}, a 9 bit book has {}{}",
            header.fileVersion, QaplaBook9::BOOK9_FILE_VERSION,
            elevenBit ? " - this is an 11 bit book, which is not supported" : ""));
    }
    // A file that is no book can still get through the header; its node count then promises more
    // than the file holds, and that is refused before anything is allocated.
    const auto available = std::filesystem::file_size(filePath) - static_cast<uintmax_t>(stream.tellg());
    if (header.nodeSize < 2 || static_cast<uintmax_t>(count) * header.nodeSize > available) {
        throw QaplaBook::BookFormatError(std::format("{} nodes of {} bytes do not fit into the file",
            count, header.nodeSize));
    }

    BookLines lines;
    lines.bits_.resize(count);
    lines.parent_.resize(count);
    // Whether a node lies below a move of priority 0, by node: such a line is never played.
    std::vector<bool> neverPlayed(count, false);
    // The parents follow from the two tree bits, as in Book::fromPreorder of the engine: one frame
    // per level still being filled, holding the node the level hangs below and whether that node has
    // a sibling of its own.
    struct Frame {
        uint32_t parent;
        bool parentHasSibling;
    };
    std::vector<Frame> frames{ Frame{ .parent = NO_NODE, .parentHasSibling = false } };
    std::vector<char> bytes(header.nodeSize);
    for (uint32_t index = 0; index < count; ++index) {
        stream.read(bytes.data(), static_cast<std::streamsize>(header.nodeSize));
        if (!stream) {
            throw QaplaBook::BookFormatError("file ends inside the node array");
        }
        if (frames.empty()) {
            throw QaplaBook::BookFormatError("preorder array has trailing nodes");
        }
        const auto bits = static_cast<uint16_t>(static_cast<uint8_t>(bytes[0])
            | (static_cast<uint8_t>(bytes[1]) << 8));
        const uint32_t parent = frames.back().parent;
        lines.bits_[index] = bits;
        lines.parent_[index] = parent;
        const Node node = nodeOf(bits);
        const bool parentNeverPlayed = parent != NO_NODE && neverPlayed[parent];
        if (!parentNeverPlayed && node.priority() == QaplaBook9::PRIORITY_NEVER) {
            ++lines.neverPlayed_;
        }
        neverPlayed[index] = parentNeverPlayed || node.priority() == QaplaBook9::PRIORITY_NEVER;
        if (node.hasChild()) {
            frames.push_back(Frame{ .parent = index, .parentHasSibling = node.hasRightSibling() });
        }
        else {
            if (!neverPlayed[index] && (!maxLines || lines.leaves_.size() < *maxLines)) {
                lines.leaves_.push_back(index);
            }
            if (!node.hasRightSibling()) {
                for (bool levelIsDone = true; levelIsDone && !frames.empty();) {
                    levelIsDone = !frames.back().parentHasSibling;
                    frames.pop_back();
                }
            }
        }
    }
    return lines;
}

GameRecord BookLines::game(size_t index) const {
    std::vector<uint32_t> chain;
    for (uint32_t node = leaves_[index]; node != NO_NODE; node = parent_[node]) {
        chain.push_back(node);
    }
    GameRecord game;
    auto board = QaplaBook9::BookBoard::startPosition();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto move = QaplaBook9::unpackMove(nodeOf(bits_[*it]).move(), board);
        MoveRecord record;
        record.original = toLan(QaplaBook9::toKingTargetCastling(move, board));
        game.addMove(record);
        board.doMove(move);
    }
    return game;
}

} // namespace QaplaTester
