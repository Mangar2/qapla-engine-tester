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

#include "../qapla-engine/book/book-io.h"
#include "../qapla-engine/book9/book-node.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief The payload id of a book whose nodes carry the search score of the position they are
 *        played from, see BookScore. Written by --bookeval version=1.
 */
inline constexpr const char* BOOK_SCORE_PAYLOAD_ID = "qapla-search-score-v1";

/**
 * @brief The payload id of a book whose nodes carry the search score of the position after their
 *        move, seen from the side that played it, see BookScore - the position a node stands for,
 *        as the counts of a book trained with games do. The start position has no node; its score,
 *        from white, is the header record BOOK_ROOT_SCORE_TAG. Written by --bookeval version=2, the
 *        default.
 */
inline constexpr const char* BOOK_SCORE_V2_PAYLOAD_ID = "qapla-search-score-v2";

/**
 * @brief The header record of the score of the start position, a decimal number coded as BookScore.
 *        A reader that does not know it skips it, as every book reader of Qapla does.
 */
inline constexpr int32_t BOOK_ROOT_SCORE_TAG = 7;

/**
 * @brief The search score a node of an evaluated book carries, two bytes, little endian, signed.
 *
 * The score is the one of the position the move of the node is played from, seen from the side to
 * move there - the evaluation a pgn comment gives a move. All moves played from one position carry
 * the same score, the position was searched once.
 *
 *   -32768                 no score: the position is not evaluated (yet)
 *   -30000 ... 30000       centipawns, as the engine reports them; beyond that range clamped
 *   31001 ... 31999        mate in 32000 - score moves for the side to move
 *  -31999 ... -31001       mated in 32000 + score moves
 *
 * The coding is symmetric: the negated score is the same position seen from the other side.
 */
struct BookScore {
    static constexpr int16_t NONE = INT16_MIN;
    static constexpr int32_t MAX_CENTIPAWNS = 30000;
    static constexpr int32_t MATE = 32000;

    [[nodiscard]] static int16_t fromSearch(std::optional<int> centipawns, std::optional<int> mateInMoves);
    [[nodiscard]] static bool isMate(int16_t score) { return score != NONE && (score > MAX_CENTIPAWNS || score < -MAX_CENTIPAWNS); }
    /** @brief Moves to mate, negative if the side to move is mated; only for a mate score. */
    [[nodiscard]] static int mateInMoves(int16_t score) { return score > 0 ? MATE - score : -(MATE + score); }
    /** @brief The score seen from the other side; no score stays no score. */
    [[nodiscard]] static int16_t negate(int16_t score) {
        return score == NONE ? NONE : static_cast<int16_t>(-score);
    }
};

/**
 * @brief The tree of a 9 bit book as it lies in the file, made for evaluating its positions.
 *
 * A position of the book is the start position or the position after the move of a node. Position
 * 0 is the start position, position i + 1 the one after node i, the nodes counted in the order of
 * the file. The tree keeps one score per position, seen from the side to move there; how it goes
 * into a file depends on the version. Version 1 needs the positions some move is played from - the
 * start position and those after a node with children - and gives the score of position p to every
 * node played from p. Version 2 needs every position: node i carries the score of position i + 1,
 * negated to the side that played the move, the start position goes into the header.
 *
 * The tree is kept as the two bytes of every node and the index of its parent: that is all that
 * is needed to name the moves leading to a position, and small enough for a book of many millions
 * of nodes.
 */
class BookTree {
public:
    static constexpr uint32_t NO_NODE = UINT32_MAX;

    /**
     * @brief Reads a 9 bit book of any payload. A book with the score payload brings its scores.
     * @throws std::runtime_error (QaplaBook::BookFormatError) if the file is no 9 bit book.
     */
    [[nodiscard]] static BookTree read(const std::string& path);

    [[nodiscard]] const QaplaBook::BookHeader& header() const { return header_; }
    [[nodiscard]] size_t nodeCount() const { return bits_.size(); }
    /** @brief Positions of the book, searched or not: the start position and one per node. */
    [[nodiscard]] size_t positionCount() const { return bits_.size() + 1; }

    /** @brief True for the positions a move is played from, the ones version 1 gives a score. */
    [[nodiscard]] bool isPlayedFrom(uint32_t position) const;

    /** @brief True for the positions a book of the version gives a score: version 2 all of them. */
    [[nodiscard]] bool isScored(uint32_t position, uint32_t version) const {
        return version == 1 ? isPlayedFrom(position) : !bits_.empty();
    }

    /** @brief 1 or 2 for a book with scores of that version, 0 for any other book. */
    [[nodiscard]] uint32_t scoreVersion() const { return scoreVersion_; }

    /** @brief The moves from the start position to a position, in long notation. */
    [[nodiscard]] std::vector<std::string> lineTo(uint32_t position) const;

    /** @brief The score of a position, BookScore::NONE if it has none. */
    [[nodiscard]] int16_t score(uint32_t position) const { return scores_[position]; }
    void setScore(uint32_t position, int16_t score) { scores_[position] = score; }

    /** @brief True if both books hold the same moves in the same tree. */
    [[nodiscard]] bool sameTree(const BookTree& other) const { return bits_ == other.bits_; }

    /**
     * @brief Writes the book with the score payload, every node carrying the score of the position
     *        its move is played from. Written beside the file and renamed then, so an interrupted
     *        write never leaves half a book behind.
     * @throws std::runtime_error if the file cannot be written.
     */
    void writeWithScores(const std::string& path, const QaplaBook::BookHeader& header,
        uint32_t version = 2) const;

private:
    /** @brief The position a node's move is played from. */
    [[nodiscard]] uint32_t positionBefore(uint32_t node) const {
        return parent_[node] == NO_NODE ? 0 : parent_[node] + 1;
    }

    QaplaBook::BookHeader header_;
    std::vector<uint16_t> bits_;      ///< The 16 bit part of every node, in the order of the file
    std::vector<uint32_t> parent_;    ///< The parent of every node, NO_NODE below the start position
    std::vector<int16_t> scores_;     ///< The score of every position, from the side to move there
    uint32_t scoreVersion_ = 0;       ///< The version of the scores the file had, 0 for none
};

} // namespace QaplaTester
