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
 * @Overview
 * Functions and Masks to work with bitboards for chess
 */

#pragma once 

#include <assert.h>
#include "types.h"
#include "evalvalue.h"
#include "move.h"
#include "bits.h"

namespace QaplaMoveGenerator {


	class BitBoardMasks
	{
	public:
		// CTor 
		BitBoardMasks() = delete;
		// DTor
		~BitBoardMasks() = delete;

		/**
		 * Computes the attack mask for pawns
		 */
		template <uint32_t COLOR>
		inline static QaplaBasics::bitBoard_t computePawnAttackMask(QaplaBasics::bitBoard_t pawns) {
			QaplaBasics::bitBoard_t attack = 
				shiftColor<COLOR, QaplaBasics::NW>(pawns) | shiftColor<COLOR, QaplaBasics::NE>(pawns);
			return attack;
		}

		/**
		 * Shifts the pawn bitboard by one move
		 */
		template<uint32_t COLOR, QaplaBasics::Square DIRECTION>
		inline static QaplaBasics::bitBoard_t shiftColor(QaplaBasics::bitBoard_t bitboard) {
			if constexpr (COLOR == QaplaBasics::WHITE) {
				return shift<DIRECTION>(bitboard);
			}
			else {
				return shift<-DIRECTION>(bitboard);
			}
		}

		/**
		 * Reflects a bitboard vertically (along the horizontal axis).
		 * Used for mirroring between white and black perspectives.
		 */
		static QaplaBasics::bitBoard_t axialReflection(QaplaBasics::bitBoard_t bitBoard) {
			QaplaBasics::bitBoard_t reflectedBitBoard = 0;
			for (QaplaBasics::File file = QaplaBasics::File::A; file < QaplaBasics::File::H; ++file) {
				reflectedBitBoard |= bitBoard & 0xFF;
				reflectedBitBoard <<= 8;
				bitBoard >>= 8;
			}
			return reflectedBitBoard;
		}

	public:
		// map from position to knight move bits
		inline static QaplaBasics::bitBoard_t knightMoves[QaplaBasics::BOARD_SIZE];
		// map from position to king move bits
		inline static QaplaBasics::bitBoard_t kingMoves[QaplaBasics::BOARD_SIZE];
		// map from position to pawn attack bits
		inline static QaplaBasics::bitBoard_t pawnCaptures[2][QaplaBasics::BOARD_SIZE];
		//static QaplaBasics::bitBoard_t mBPawnCaptures[BOARD_SIZE];
		// map from pawn-target position to adjacent bits on ep file
		inline static QaplaBasics::bitBoard_t EPMask[QaplaBasics::BOARD_SIZE];

		/**
		 * Ray bitboards from any square to any other square along a line (rook or bishop direction),
		 * excluding the origin square, including the target square.
		 *
		 * Indexed as: mRay[from + to * 64]
		 */
		inline static QaplaBasics::bitBoard_t Ray[QaplaBasics::BOARD_SIZE * QaplaBasics::BOARD_SIZE];

		/**
		 * Full ray bitboards between two squares, if they lie on the same rank, file, or diagonal.
		 *
		 * The full ray includes *all* squares on the line connecting the two squares, not just those
		 * between them. This means the entire rank/file/diagonal is marked if both squares are aligned.
		 *
		 * Used for pin and x-ray detection, e.g. to test if a piece is between king and attacker,
		 * or to collect all squares that must remain blocked to avoid exposing the king.
		 *
		 * Example: for squares B1 and D1 (same rank), the full ray is A1-H1.
		 *
		 * Indexed as: mFullRay[from + to * 64]
		 */
		inline static QaplaBasics::bitBoard_t FullRay[QaplaBasics::BOARD_SIZE * QaplaBasics::BOARD_SIZE];

		// ---------------------- Helpers -----------------------------------------
		
		/**
		 * Generates all possible targets for a knight
		 */
		static QaplaBasics::bitBoard_t genKnightTargetBoard(QaplaBasics::Square square);

		/**
		 * Generates all possible targets for a king
		 */
		static QaplaBasics::bitBoard_t genKingTargetBoard(QaplaBasics::Square square);

		static constexpr QaplaBasics::bitBoard_t RANK_1_BITMASK = 0x00000000000000FF;
		static constexpr QaplaBasics::bitBoard_t RANK_2_BITMASK = 0x000000000000FF00;
		static constexpr QaplaBasics::bitBoard_t RANK_3_BITMASK = 0x0000000000FF0000;
		static constexpr QaplaBasics::bitBoard_t RANK_4_BITMASK = 0x00000000FF000000;
		static constexpr QaplaBasics::bitBoard_t RANK_5_BITMASK = 0x000000FF00000000;
		static constexpr QaplaBasics::bitBoard_t RANK_6_BITMASK = 0x0000FF0000000000;
		static constexpr QaplaBasics::bitBoard_t RANK_7_BITMASK = 0x00FF000000000000;
		static constexpr QaplaBasics::bitBoard_t RANK_8_BITMASK = 0xFF00000000000000;

		static constexpr QaplaBasics::bitBoard_t FILE_A_BITMASK = 0x0101010101010101;
		static constexpr QaplaBasics::bitBoard_t FILE_B_BITMASK = 0x0202020202020202;
		static constexpr QaplaBasics::bitBoard_t FILE_C_BITMASK = 0x0404040404040404;
		static constexpr QaplaBasics::bitBoard_t FILE_D_BITMASK = 0x0808080808080808;
		static constexpr QaplaBasics::bitBoard_t FILE_E_BITMASK = 0x1010101010101010;
		static constexpr QaplaBasics::bitBoard_t FILE_F_BITMASK = 0x2020202020202020;
		static constexpr QaplaBasics::bitBoard_t FILE_G_BITMASK = 0x4040404040404040;
		static constexpr QaplaBasics::bitBoard_t FILE_H_BITMASK = 0x8080808080808080;

		static constexpr QaplaBasics::bitBoard_t fileBB[8] = {
			FILE_A_BITMASK, FILE_B_BITMASK, FILE_C_BITMASK, FILE_D_BITMASK,
			FILE_E_BITMASK, FILE_F_BITMASK, FILE_G_BITMASK, FILE_H_BITMASK
		};

		/**
		 * Shifts all bits of the given bitboard in the specified direction,
		 * handling edge file masking to avoid wrap-around effects.
		 *
		 * Template parameter DIRECTION must be one of: NORTH, SOUTH, EAST, WEST, NW, NE, SW, SE.
		 */
		template<QaplaBasics::Square DIRECTION>
		constexpr static QaplaBasics::bitBoard_t shift(QaplaBasics::bitBoard_t bitBoard) {
			switch (DIRECTION) {
			case QaplaBasics::NORTH: return bitBoard << QaplaBasics::NORTH;
			case QaplaBasics::NORTH_2: return bitBoard << (QaplaBasics::NORTH * 2);
			case QaplaBasics::SOUTH: return bitBoard >> -QaplaBasics::SOUTH;
			case QaplaBasics::SOUTH_2: return bitBoard >> (-QaplaBasics::SOUTH * 2);
			case QaplaBasics::EAST: return (bitBoard & ~FILE_H_BITMASK) << QaplaBasics::EAST;
			case QaplaBasics::WEST: return (bitBoard & ~FILE_A_BITMASK) >> -QaplaBasics::WEST;
			case QaplaBasics::NW: return (bitBoard & ~FILE_A_BITMASK) << QaplaBasics::NW;
			case QaplaBasics::NE: return (bitBoard & ~FILE_H_BITMASK) << QaplaBasics::NE;
			case QaplaBasics::SW: return (bitBoard & ~FILE_A_BITMASK) >> -QaplaBasics::SW;
			case QaplaBasics::SE: return (bitBoard & ~FILE_H_BITMASK) >> -QaplaBasics::SE;
			default: return bitBoard;
			}
		}


		/**
		 * Logical or of a bitboard moved to all 4 directions
		 */
		inline static QaplaBasics::bitBoard_t moveInAllDirections(QaplaBasics::bitBoard_t board) {
			board |= shift<QaplaBasics::WEST>(board) | shift<QaplaBasics::EAST>(board);
			board |= shift<QaplaBasics::NORTH>(board) | shift<QaplaBasics::SOUTH>(board);
			return board;
		}

	private:
		/**
		 * Initializes precomputed rays between all square pairs used for pin and x-ray detection.
		 */
		static void initAttackRay();

		// Initializes masks
		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

	};
}

