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
 * @author Volker Boehm
 * @copyright Copyright (c) 2021 Volker Boehm
 * @Overview
 * Implements bit handling routines
 */

#pragma once

#include <assert.h>
#include <bit>
#include "types.h"

namespace QaplaBasics {

	constexpr uint32_t popCount(bitBoard_t bitBoard) {
		return static_cast<uint32_t>(std::popcount(bitBoard));
	}

	constexpr uint32_t popCountForSparcelyPopulatedBitBoards(bitBoard_t bitBoard) {
		return popCount(bitBoard);
	}

	constexpr Square lsb(bitBoard_t bitBoard) {
		assert(bitBoard != 0);
		return static_cast<Square>(std::countr_zero(bitBoard));
	}

	/**
	 * Removes the least significant bit
	 */
	constexpr Square popLSB(bitBoard_t& bitBoard)
	{
		const Square res = lsb(bitBoard);
		bitBoard &= bitBoard - 1;
		return res;
	}
}

