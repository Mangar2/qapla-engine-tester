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
 * Implements piece square table for static evaluation for the piece placement
 */

#ifndef __PST_H
#define __PST_H

#include <vector>
#include "types.h"
#include "evalvalue.h"

namespace QaplaBasics {
	class PST
	{
	public:
		/**
		 * Gets a value from the piece square tables
		 */
		static EvalValue getValue(Square square, Piece piece) { return _pst[piece][square]; }
		static std::vector<EvalValue> getPSTLookup(Piece piece) { return std::vector<EvalValue>(_pst[piece], _pst[piece] + BOARD_SIZE); }
	private:

		/**
		 * Initializes the piece square table
		 */
		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		static EvalValue _pst[PIECE_AMOUNT][BOARD_SIZE];

		constexpr static value_t PAWN_PST[][int(File::COUNT)][2] = {
			{ {   0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {   0,  0 }, {   0,  0 } },
			{ {   0,  0 }, {  0,  0 }, {  5,  0 }, {  5,  0 }, {  5,  0 }, {  5,  0 }, {   0,  0 }, {   0,  0 } },
			{ {  -5,  0 }, { -5,  0 }, {  5,  0 }, { 10,  0 }, { 10,  0 }, {  5,  0 }, {  -5,  0 }, {  -5,  0 } },
			{ { -10,  0 }, { -5,  0 }, { 10,  0 }, { 20,  0 }, { 20,  0 }, { 10,  0 }, { -10,  0 }, { -10,  0 } },
			{ { -10,  0 }, { -5,  0 }, {  0,  0 }, { 10,  0 }, { 10,  0 }, {  0,  0 }, { -10,  0 }, { -10,  0 } },
			{ { -10,  0 }, { -5,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, { -10,  0 }, { -10,  0 } },
			{ {   0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {   0,  0 }, {   0,  0 } },
			{ {   0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {   0,  0 }, {   0,  0 } }
		};

		constexpr static value_t KNIGHT_PST[][int(File::COUNT) / 2][2] = {
			{ { -100, -50 }, { -50, -30 }, { -40, -20 }, { -40, -10 } },
			{ {  -30, -40 }, { -20, -25 }, { -10, -10 }, {  -5,   2 } },
			{ {  -20, -30 }, {  -5, -10 }, {   2,  -2 }, {   5,  10 } },
			{ {  -10, -20 }, {   2,   0 }, {  15,   5 }, {  25,  15 } },
			{ {  -10, -20 }, {   5,  -5 }, {  20,   5 }, {  25,  15 } },
			{ {   -2, -30 }, {  10, -15 }, {  25,  -5 }, {  25,  10 } },
			{ {  -30, -40 }, { -10, -25 }, {   0, -25 }, {  10,   2 } },
			{ { -100, -50 }, { -40, -40 }, { -25, -25 }, { -10, -10 } }
		};

		constexpr static value_t BISHOP_PST[][int(File::COUNT) / 2][2] = {
			{ { -20, -20 }, {  0, -15 }, { -5, -15 }, { -10, -10 } },
			{ { -10, -15 }, {  5,  -5 }, { 10,  -5 }, {   0,   0 } },
			{ {  -5, -10 }, { 10,   0 }, {  0,   0 }, {  10,   5 } },
			{ {  -5,  -5 }, {  5,   0 }, { 10,   0 }, {  15,   5 } },
			{ {  -5,  -5 }, { 10,   0 }, { 10,   0 }, {  15,   5 } },
			{ {  -5, -10 }, {  0,   0 }, {  0,   0 }, {   5,   2 } },
			{ { -10, -15 }, { -5,  -5 }, {  0,  -5 }, {   0,   0 } },
			{ { -20, -20 }, { -5, -15 }, { -5, -15 }, { -10, -10 } }
		};

		constexpr static value_t ROOK_PST[][int(File::COUNT) / 2][2] = {
			{ { -15, -5 }, { -10, -5 }, {  -5, -5 }, { -2, -5 } },
			{ { -10, -5 }, {  -5, -5 }, {  -2,  0 }, {  2,  0 } },
			{ { -10, -2 }, {  -5, -2 }, {   0,  0 }, {  2,  0 } },
			{ { -10, -2 }, {  -5,  0 }, {   0,  0 }, {  2,  0 } },
			{ { -10, -2 }, {  -5,  0 }, {   0,  0 }, {  2,  0 } },
			{ { -10,  0 }, {   0,  0 }, {   2,  0 }, {  5,  2 } },
			{ {   5,  2 }, {   5,  2 }, {   5,  5 }, {  5,  2 } },
			{ { -10,  5 }, { -10,  5 }, {   0,  5 }, {  0,  5 } },
		};

		constexpr static value_t QUEEN_PST[][int(File::COUNT) / 2][2] = {
			{ { 0, -20 }, { 0, -20 }, { 0, -15 }, { 0, -10 } },
			{ { 0, -20 }, { 2, -15 }, { 5, -10 }, { 5,   0 } },
			{ { 0, -15 }, { 5, -10 }, { 5,  -5 }, { 5,   0 } },
			{ { 0, -10 }, { 5,   0 }, { 5,   5 }, { 5,  10 } },
			{ { 0, -10 }, { 5,   0 }, { 5,   5 }, { 5,  10 } },
			{ { 0, -15 }, { 5, -10 }, { 5,  -5 }, { 5,   0 } },
			{ { 0, -20 }, { 2, -15 }, { 5, -10 }, { 5,  -5 } },
			{ { 0, -20 }, { 0, -20 }, { 0, -15 }, { 0, -15 } },
		};

		constexpr static value_t KING_PST[][int(File::COUNT) / 2][2] = {
			{ { 50,  0 }, { 60,  2 }, { 40,  5 }, { 30,  5 } },
			{ { 55,  5 }, { 55, 10 }, { 30, 25 }, { 25, 25 } },
			{ { 40, 10 }, { 45, 25 }, { 25, 35 }, { 15, 35 } },
			{ { 30, 20 }, { 35, 35 }, { 20, 40 }, { 10, 40 } },
			{ { 20, 20 }, { 25, 35 }, { 15, 40 }, {  5, 40 } },
			{ { 10, 10 }, { 15, 25 }, { 10, 35 }, {  2, 35 } },
			{ {  5,  5 }, { 10, 10 }, {  5, 20 }, {  0, 20 } },
			{ {  0,  0 }, {  0,  2 }, {  0,  5 }, {  0,  5 } },
		};

	};
}

#endif  // __PST_H
