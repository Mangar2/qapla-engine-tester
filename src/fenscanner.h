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
 * Scans a fen string
 */

#pragma once

#include "types.h"
#include <string>
#include <array>

#include "movegenerator.h"

using namespace QaplaBasics;

namespace QaplaInterface {
	class FenScanner {
	public:
		FenScanner() {
			error = true;
		}

		bool setBoard(std::string fen, QaplaMoveGenerator::MoveGenerator& position) {
			position.clear();
			std::string::iterator fenIterator = fen.begin();
			error = false;

			scanPieceSector(fen, fenIterator, position);
			skipBlank(fen, fenIterator);
			scanSideToMove(fen, fenIterator, position);

			if (!error && skipBlank(fen, fenIterator)) {
				scanCastlingRights(fen, fenIterator, position);
			}

			if (!error && skipBlank(fen, fenIterator)) {
				scanEPField(fen, fenIterator, position);
			}

			if (!error && skipBlank(fen, fenIterator)) {
				scanHalfMovesWithouthPawnMoveOrCapture(fen, fenIterator, position);
			}

			if (!error && skipBlank(fen, fenIterator)) {
				scanPlayedMovesInGame(fen, fenIterator, position);
			}

			if (!error) {
				//position->finishBoardSetup();
			}

			return !error;
		}

	private:

		bool error;

		/**
		 * Scans the piece sector of a fen std::string
		 */
		void scanPieceSector(const std::string& fen, std::string::iterator& fenIterator, QaplaMoveGenerator::MoveGenerator& chessBoard) {

			int16_t file = 0;
			int16_t rank = 7;

			for (; fenIterator != fen.end() && !error && rank >= 0; ++fenIterator) {
				char curChar = *fenIterator;
				if (curChar == 0 || curChar == ' ') {
					break;
				}
				else if (curChar == '/') {
					error = file != NORTH;
					file = 0;
					rank--;
				}
				else if (isPieceChar(curChar)) {
					chessBoard.setPiece(
						computeSquare(static_cast<QaplaBasics::File>(file), static_cast<QaplaBasics::Rank>(rank)), 
						QaplaBasics::charToPiece(curChar));
					file++;
				}
				else if (isColChar(curChar)) {
					file += (curChar - '0');
				}
				else {
					error = true;
				}
			}

			if (file != NORTH || rank != 0) {
				error = true;
			}

		}

		/**
		 * Skips a mandatory blank
		 */
		bool skipBlank(const std::string& fen, std::string::iterator& fenIterator) {
			bool hasBlank = false;
			if (fenIterator != fen.end()) {
				if (*fenIterator == ' ') {
					++fenIterator;
					hasBlank = true;
				}
			}
			return hasBlank;
		}

		/**
		 * Scans the side to move, either "w" or "b", default white
		 */
		void scanSideToMove(const std::string& fen, std::string::iterator& fenIterator, QaplaMoveGenerator::MoveGenerator& chessBoard) {
			if (fenIterator != fen.end()) {
				chessBoard.setWhiteToMove(*fenIterator == 'w');
				if (*fenIterator != 'w' && *fenIterator != 'b') {
					error = true;
				}
				++fenIterator;
			}
		}

		/**
		 * Scans the castling rights section 'K', 'Q' for white rights and 'k', 'q' for black rights
		 * Or '-' for no castling rights
		 */
		void scanCastlingRights(const std::string& fen, std::string::iterator& fenIterator, QaplaMoveGenerator::MoveGenerator& chessBoard) {
			bool castlingRightsFound = false;

			// Default: every castle right activated
			if (fenIterator == fen.end()) {
				chessBoard.setCastlingRight(WHITE, true, true);
				chessBoard.setCastlingRight(WHITE, false, true);
				chessBoard.setCastlingRight(BLACK, true, true);
				chessBoard.setCastlingRight(BLACK, false, true);
			}
			else {
				if (*fenIterator == 'K') {
					chessBoard.setCastlingRight(WHITE, true, true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == 'Q') {
					chessBoard.setCastlingRight(WHITE, false, true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == 'k') {
					chessBoard.setCastlingRight(BLACK, true, true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == 'q') {
					chessBoard.setCastlingRight(BLACK, false, true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == '-') {
					if (castlingRightsFound) {
						error = true;
					}
					++fenIterator;
				}
				else if (!castlingRightsFound) {
					error = true;
				}
			}
		}

		/**
		 * Scans an EN-Passant-Field
		 */
		void scanEPField(const std::string& fen, std::string::iterator& fenIterator, QaplaMoveGenerator::MoveGenerator& chessBoard) {
			uint32_t epFile = -1;
			uint32_t epRank = -1;
			if (fenIterator != fen.end() && *fenIterator == '-') {
				++fenIterator;
				return;
			}
			if (fenIterator != fen.end() && *fenIterator >= 'a' && *fenIterator <= 'h') {
				epFile = *fenIterator - 'a';
				++fenIterator;
			}
			if (fenIterator != fen.end() && (*fenIterator == '3' || *fenIterator == '6')) {
				epRank = *fenIterator - '1';
				++fenIterator;
			}
			if (epFile != -1 && epRank != -1) {
				chessBoard.setEP(computeSquare(static_cast<QaplaBasics::File>(epFile), static_cast<QaplaBasics::Rank>(epRank)));
			}
			else {
				// No legal EN-Passant-Field found
				error = true;
			}
		}

		/**
		 * Scans a positive integer in the fen
		 */
		uint32_t scanInteger(const std::string& fen, std::string::iterator& fenIterator) {
			uint32_t result = 0;
			while (fenIterator != fen.end() && *fenIterator >= '0' && *fenIterator <= '9') {
				result *= 10;
				result += *fenIterator - '0';
				++fenIterator;
			}
			return result;
		}

		void scanHalfMovesWithouthPawnMoveOrCapture(const std::string& fen, std::string::iterator& fenIterator, QaplaMoveGenerator::MoveGenerator& chessBoard) {
			chessBoard.setHalfmovesWithoutPawnMoveOrCapture((uint8_t)scanInteger(fen, fenIterator));
		}

		void scanPlayedMovesInGame(const std::string& fen, std::string::iterator& fenIterator, QaplaMoveGenerator::MoveGenerator& chessBoard) {
			//chessBoard.setPlayedMovesInGame(scanInteger(fen, fenIterator));
		}

		bool isPieceChar(char pieceChar) {
			std::string supportedChars = "PpNnBbRrQqKk";
			return supportedChars.find(pieceChar) != std::string::npos;
		}

		bool isColChar(char colChar) {
			std::string supportedChars = "12345678";
			return supportedChars.find(colChar) != std::string::npos;
		}

	};
}
