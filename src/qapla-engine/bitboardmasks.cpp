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
 * @copyright Copyright (c) 2021 Volker Böhm
 */

#include "bitboardmasks.h"

using namespace QaplaMoveGenerator;

BitBoardMasks::InitStatics BitBoardMasks::_staticConstructor;


// -------------------------- GenKingTargetBoard ------------------------------
QaplaBasics::bitBoard_t BitBoardMasks::genKingTargetBoard(QaplaBasics::Square square)
{
	QaplaBasics::bitBoard_t squareBB = 1ULL << square;

	QaplaBasics::bitBoard_t aResult;
	aResult = shift<QaplaBasics::NW>(squareBB) | shift<QaplaBasics::NORTH>(squareBB) | shift<QaplaBasics::NE>(squareBB) |
		shift<QaplaBasics::EAST>(squareBB) | shift<QaplaBasics::WEST>(squareBB) |
		shift<QaplaBasics::SW>(squareBB) | shift<QaplaBasics::SOUTH>(squareBB) | shift<QaplaBasics::SE>(squareBB);

	return aResult;
}

// -------------------------- GenKnightTargetBoard ----------------------------
QaplaBasics::bitBoard_t BitBoardMasks::genKnightTargetBoard(QaplaBasics::Square square)
{
	QaplaBasics::bitBoard_t squareBB = 1ULL << square;

	return
		shift<QaplaBasics::NW>(shift<QaplaBasics::NORTH>(squareBB)) |
		shift<QaplaBasics::NE>(shift<QaplaBasics::NORTH>(squareBB)) |
		shift<QaplaBasics::NW>(shift<QaplaBasics::WEST>(squareBB)) |
		shift<QaplaBasics::NE>(shift<QaplaBasics::EAST>(squareBB)) |
		shift<QaplaBasics::SW>(shift<QaplaBasics::SOUTH>(squareBB)) |
		shift<QaplaBasics::SE>(shift<QaplaBasics::SOUTH>(squareBB)) |
		shift<QaplaBasics::SW>(shift<QaplaBasics::WEST>(squareBB)) |
		shift<QaplaBasics::SE>(shift<QaplaBasics::EAST>(squareBB));
}

// -------------------------- InitStatics -------------------------------------


void BitBoardMasks::initAttackRay() {
	QaplaBasics::Square square;
	QaplaBasics::Square square2;
	int32_t dir;

	const int32_t MOVE_DIRECTION[8][2] =
		{ { 1,0 }, { -1,0 }, { 0,1 }, { 0,-1 }, { 1,1 }, { 1,-1 }, { -1,1 }, { -1,-1 } };


	for (square = QaplaBasics::A1; square <= QaplaBasics::H8; ++square)
	{
		for (square2 = QaplaBasics::A1; square2 <= QaplaBasics::H8; ++square2)
		{
			// first initializes the ray, only setting target bit
			Ray[square + square2 * 64] = 1ULL << (square + square2 * 64);
			// full ray ist set to zero
			FullRay[square + square2 * 64] = 0ULL;
		}
	}
	for (square = QaplaBasics::A1; square <= QaplaBasics::H8; ++square)
	{
		// Second loop through colums and diagonals
		for (dir = 0; dir < 8; dir++)
		{
			QaplaBasics::bitBoard_t aBoard = 0;
			QaplaBasics::File file = getFile(square) + MOVE_DIRECTION[dir][0];
			QaplaBasics::Rank rank = getRank(square) + MOVE_DIRECTION[dir][1];
			for (; isFileInBoard(file) && isRankInBoard(rank); file += MOVE_DIRECTION[dir][0], rank += MOVE_DIRECTION[dir][1])
			{
				aBoard |= 1ULL << computeSquare(file, rank);
				Ray[square + computeSquare(file, rank) * 64] = aBoard;
			}
			// The board has stored the full ray to the border of the board, set it to every field in ray
			file = getFile(square) + MOVE_DIRECTION[dir][0];
			rank = getRank(square) + MOVE_DIRECTION[dir][1];
			for (; isFileInBoard(file) && isRankInBoard(rank); file += MOVE_DIRECTION[dir][0], rank += MOVE_DIRECTION[dir][1])
			{
				FullRay[square + computeSquare(file, rank) * 64] = aBoard;
			}

		}
	}
}

BitBoardMasks::InitStatics::InitStatics()
{

	QaplaBasics::Square square;

	initAttackRay();
	for (square = QaplaBasics::A1; square <= QaplaBasics::H8; ++square)
	{
		knightMoves[square] = genKnightTargetBoard(square);
		kingMoves[square] = genKingTargetBoard(square);
		// Set pawn capture masks
		pawnCaptures[QaplaBasics::WHITE][square] = 0;
		pawnCaptures[QaplaBasics::BLACK][square] = 0;
		EPMask[square] = 0;
		if (getRank(square) > QaplaBasics::Rank::R1 && getRank(square) < QaplaBasics::Rank::R8)
		{
			pawnCaptures[QaplaBasics::WHITE][square] = 5ULL << (square + 7);
			pawnCaptures[QaplaBasics::BLACK][square] = 5ULL << (square - 9);
			EPMask[square] = 5ULL << (square - 1);
			if (getFile(square) == QaplaBasics::File::A || getFile(square) == QaplaBasics::File::H)
			{
				pawnCaptures[QaplaBasics::WHITE][square] &= ~(FILE_H_BITMASK | FILE_A_BITMASK);
				pawnCaptures[QaplaBasics::BLACK][square] &= ~(FILE_H_BITMASK | FILE_A_BITMASK);
				EPMask[square] &= ~(FILE_H_BITMASK | FILE_A_BITMASK);
			}
		}
		EPMask[square] = 0ULL;
	}
	for (square = QaplaBasics::A4; square < QaplaBasics::A6; ++square)
	{
		EPMask[square] = 0x05ULL << (square - 1);
		EPMask[square] &= 0XFFULL << (square - (square % QaplaBasics::NORTH));
	}

}
