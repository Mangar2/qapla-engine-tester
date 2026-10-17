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
#include <charconv>
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

/** The longest string a header record may hold, a broken file must not allocate. */
constexpr int32_t MAX_RECORD_LENGTH = 1 << 16;

int32_t readInt32(std::istream& stream) {
    int32_t value = 0;
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!stream) {
        throw QaplaBook::BookFormatError("file ends inside the header");
    }
    return value;
}

void writeInt32(std::ostream& stream, int32_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void writeRecord(std::ostream& stream, int32_t tag, const std::string& value) {
    writeInt32(stream, tag);
    writeInt32(stream, static_cast<int32_t>(value.size() + 1));
    stream.write(value.c_str(), static_cast<std::streamsize>(value.size() + 1));
}

/**
 * @brief Reads the header as QaplaBook::readBookHeader does, and the record of the score of the
 *        start position beside it, which that one skips.
 * @return The number of nodes.
 */
uint32_t readHeader(std::istream& stream, QaplaBook::BookHeader& header, int16_t& rootScore) {
    using Tag = QaplaBook::BookHeaderTag;
    header = QaplaBook::BookHeader{};
    header.fileVersion = "1";
    header.nodeSize = 2;
    rootScore = BookScore::NONE;
    for (int32_t tag = readInt32(stream); tag != static_cast<int32_t>(Tag::END_OF_HEADER); tag = readInt32(stream)) {
        const int32_t length = readInt32(stream);
        if (length < 0 || length > MAX_RECORD_LENGTH) {
            throw QaplaBook::BookFormatError("header record with an implausible length");
        }
        std::string value(static_cast<size_t>(length), '\0');
        stream.read(value.data(), length);
        if (!stream) {
            throw QaplaBook::BookFormatError("file ends inside a header record");
        }
        if (!value.empty() && value.back() == '\0') {
            value.pop_back();
        }
        switch (tag) {
            case static_cast<int32_t>(Tag::AUTHOR): header.author = value; break;
            case static_cast<int32_t>(Tag::NAME): header.name = value; break;
            case static_cast<int32_t>(Tag::BOOK_VERSION): header.version = value; break;
            case static_cast<int32_t>(Tag::FILE_VERSION): header.fileVersion = value; break;
            case static_cast<int32_t>(Tag::PAYLOAD_ID): header.payloadId = value; break;
            case static_cast<int32_t>(Tag::NODE_SIZE): {
                uint32_t size = 0;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), size);
                if (result.ec != std::errc() || size == 0) {
                    throw QaplaBook::BookFormatError("node size record is not a number");
                }
                header.nodeSize = size;
                break;
            }
            case BOOK_ROOT_SCORE_TAG: {
                int score = 0;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), score);
                if (result.ec != std::errc() || score < INT16_MIN || score > INT16_MAX) {
                    throw QaplaBook::BookFormatError("score record of the start position is not a number");
                }
                rootScore = static_cast<int16_t>(score);
                break;
            }
            default:
                break;  // unknown records are skipped
        }
    }
    const int32_t count = readInt32(stream);
    if (count < 0) {
        throw QaplaBook::BookFormatError("negative node count");
    }
    return static_cast<uint32_t>(count);
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
    int16_t rootScore = BookScore::NONE;
    const uint32_t count = readHeader(stream, tree.header_, rootScore);
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
    if (nodeSize >= 4 && tree.header_.payloadId == BOOK_SCORE_PAYLOAD_ID) {
        tree.scoreVersion_ = 1;
    } else if (nodeSize >= 4 && tree.header_.payloadId == BOOK_SCORE_V2_PAYLOAD_ID) {
        tree.scoreVersion_ = 2;
    }

    tree.bits_.resize(count);
    tree.parent_.resize(count);
    tree.scores_.assign(static_cast<size_t>(count) + 1, BookScore::NONE);
    if (tree.scoreVersion_ == 2) {
        tree.scores_[0] = rootScore;
    }

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
        const auto payload = static_cast<int16_t>(byte(2) | (byte(3) << 8));
        if (tree.scoreVersion_ == 1) {
            tree.scores_[tree.positionBefore(index)] = payload;
        } else if (tree.scoreVersion_ == 2) {
            // The node carries the position after its move from the side that played it.
            tree.scores_[index + 1] = BookScore::negate(payload);
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

void BookTree::writeWithScores(const std::string& path, const QaplaBook::BookHeader& header,
    uint32_t version) const {
    const std::string partPath = path + ".part";
    {
        std::ofstream stream(partPath, std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("cannot write " + partPath);
        }
        using Tag = QaplaBook::BookHeaderTag;
        writeRecord(stream, static_cast<int32_t>(Tag::AUTHOR), header.author);
        writeRecord(stream, static_cast<int32_t>(Tag::NAME), header.name);
        writeRecord(stream, static_cast<int32_t>(Tag::BOOK_VERSION), header.version);
        writeRecord(stream, static_cast<int32_t>(Tag::FILE_VERSION), QaplaBook9::BOOK9_FILE_VERSION);
        writeRecord(stream, static_cast<int32_t>(Tag::PAYLOAD_ID),
            version == 1 ? BOOK_SCORE_PAYLOAD_ID : BOOK_SCORE_V2_PAYLOAD_ID);
        writeRecord(stream, static_cast<int32_t>(Tag::NODE_SIZE), "4");
        if (version == 2 && !scores_.empty() && scores_[0] != BookScore::NONE) {
            writeRecord(stream, BOOK_ROOT_SCORE_TAG, std::to_string(scores_[0]));
        }
        writeInt32(stream, static_cast<int32_t>(Tag::END_OF_HEADER));
        writeInt32(stream, static_cast<int32_t>(bits_.size()));
        std::vector<char> data;
        data.reserve(bits_.size() * 4);
        for (uint32_t index = 0; index < bits_.size(); ++index) {
            const auto bits = bits_[index];
            const auto score = static_cast<uint16_t>(version == 1 ? scores_[positionBefore(index)]
                : BookScore::negate(scores_[index + 1]));
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
