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

#include "book-tree.h"

#include "../qapla-engine/book9/book.h"
#include "../qapla-engine/types.h"

#include <algorithm>
#include <cstring>
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

int16_t BookScore::fromSearch(std::optional<int> centipawns, std::optional<int> mateInMoves) {
    if (mateInMoves) {
        const int moves = std::clamp(*mateInMoves < 0 ? -*mateInMoves : *mateInMoves, 1, 999);
        return static_cast<int16_t>(*mateInMoves < 0 ? -(MATE - moves) : MATE - moves);
    }
    if (centipawns) {
        return static_cast<int16_t>(std::clamp(*centipawns, -MAX_CENTIPAWNS, MAX_CENTIPAWNS));
    }
    return NONE;
}

BookTree BookTree::read(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw QaplaBook::BookFormatError("cannot open " + path);
    }
    BookTree tree;
    const uint32_t count = QaplaBook::readBookHeader(stream, tree.header_);
    if (tree.header_.fileVersion != QaplaBook9::BOOK9_FILE_VERSION) {
        throw QaplaBook::BookFormatError(std::format("{} has file version {}, a 9 bit book has {}",
            path, tree.header_.fileVersion, QaplaBook9::BOOK9_FILE_VERSION));
    }
    const uint32_t nodeSize = tree.header_.nodeSize;
    const auto available = std::filesystem::file_size(path) - static_cast<uintmax_t>(stream.tellg());
    if (nodeSize < 2 || static_cast<uintmax_t>(count) * nodeSize > available) {
        throw QaplaBook::BookFormatError(std::format("{}: {} nodes of {} bytes do not fit into the file",
            path, count, nodeSize));
    }
    const bool withScores = tree.header_.payloadId == BOOK_SCORE_PAYLOAD_ID && nodeSize >= 4;

    tree.bits_.resize(count);
    tree.parent_.resize(count);
    tree.scores_.assign(static_cast<size_t>(count) + 1, BookScore::NONE);

    // The parents follow from the two tree bits, as in Book::fromPreorder of the engine: one frame
    // per level still being filled, holding the node the level hangs below and whether that node
    // has a sibling of its own.
    struct Frame {
        uint32_t parent;
        bool parentHasSibling;
    };
    std::vector<Frame> frames{ Frame{ .parent = NO_NODE, .parentHasSibling = false } };
    std::vector<char> bytes(nodeSize);
    for (uint32_t index = 0; index < count; ++index) {
        stream.read(bytes.data(), static_cast<std::streamsize>(nodeSize));
        if (!stream) {
            throw QaplaBook::BookFormatError(path + " ends inside the node array");
        }
        if (frames.empty()) {
            throw QaplaBook::BookFormatError(path + ": nodes behind the end of the tree");
        }
        const auto byte = [&](size_t at) { return static_cast<uint8_t>(bytes[at]); };
        const auto bits = static_cast<uint16_t>(byte(0) | (byte(1) << 8));
        tree.bits_[index] = bits;
        tree.parent_[index] = frames.back().parent;
        if (withScores) {
            tree.scores_[tree.positionBefore(index)] = static_cast<int16_t>(byte(2) | (byte(3) << 8));
        }
        const Node node = nodeOf(bits);
        if (node.hasChild()) {
            frames.push_back(Frame{ .parent = index, .parentHasSibling = node.hasRightSibling() });
        }
        else if (!node.hasRightSibling()) {
            for (bool levelIsDone = true; levelIsDone && !frames.empty();) {
                levelIsDone = !frames.back().parentHasSibling;
                frames.pop_back();
            }
        }
    }
    if (count != 0 && !frames.empty()) {
        throw QaplaBook::BookFormatError(path + ": the tree ends inside the children of a node");
    }
    return tree;
}

bool BookTree::isPlayedFrom(uint32_t position) const {
    if (position == 0) {
        return !bits_.empty();
    }
    return nodeOf(bits_[position - 1]).hasChild();
}

std::vector<std::string> BookTree::lineTo(uint32_t position) const {
    std::vector<uint32_t> chain;
    for (uint32_t node = position == 0 ? NO_NODE : position - 1; node != NO_NODE; node = parent_[node]) {
        chain.push_back(node);
    }
    std::vector<std::string> line;
    line.reserve(chain.size());
    auto board = QaplaBook9::BookBoard::startPosition();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const auto move = QaplaBook9::unpackMove(nodeOf(bits_[*it]).move(), board);
        line.push_back(toLan(QaplaBook9::toKingTargetCastling(move, board)));
        board.doMove(move);
    }
    return line;
}

void BookTree::writeWithScores(const std::string& path, const QaplaBook::BookHeader& header) const {
    const std::string partPath = path + ".part";
    {
        std::ofstream stream(partPath, std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("cannot write " + partPath);
        }
        QaplaBook::BookHeader written = header;
        written.fileVersion = QaplaBook9::BOOK9_FILE_VERSION;
        written.payloadId = BOOK_SCORE_PAYLOAD_ID;
        written.nodeSize = 4;
        QaplaBook::writeBookHeader(stream, written, static_cast<uint32_t>(bits_.size()));
        std::vector<char> data;
        data.reserve(bits_.size() * 4);
        for (uint32_t index = 0; index < bits_.size(); ++index) {
            const auto bits = bits_[index];
            const auto score = static_cast<uint16_t>(scores_[positionBefore(index)]);
            data.push_back(static_cast<char>(bits & 0xFF));
            data.push_back(static_cast<char>(bits >> 8));
            data.push_back(static_cast<char>(score & 0xFF));
            data.push_back(static_cast<char>(score >> 8));
        }
        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!stream) {
            throw std::runtime_error("writing " + partPath + " failed");
        }
    }
    std::filesystem::rename(partPath, path);
}

} // namespace QaplaTester
