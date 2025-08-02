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
 * Bitboard based move generator using magic numbers
 * Generates only fully legal moves
 */

#pragma once

#include "types.h"
#include "move.h"
#include "movelist.h"
#include "board.h"
#include "magics.h"



// ----------------------------------------------------------------------------
// Implements move generator
// ----------------------------------------------------------------------------

namespace QaplaMoveGenerator {

	class MoveGenerator : public QaplaBasics::Board
	{
	public:
		MoveGenerator(void);

		/**
		 * Checks if the king of the side to move is currently under attack.
		 *
		 * This uses the precomputed attack masks and tests whether the moving side's
		 * king is on any square attacked by the opponent.
		 *
		 * @return True if the side to move is in check, false otherwise.
		 */
		inline bool isInCheck() const {
			bool result;
			if (isWhiteToMove()) {
				result = (bitBoardsPiece[QaplaBasics::WHITE_KING] & attackMask[QaplaBasics::BLACK]) != 0;
			}
			else {
				result = (bitBoardsPiece[QaplaBasics::BLACK_KING] & attackMask[QaplaBasics::WHITE]) != 0;
			}
			return result;
		}

		/**
		 * Checks if the board state is legal: both kings exist and the king
		 * not on move is not in check (used after move application).
		 *
		 * @return True if the position is legal.
		 */
		bool isLegal() {
			computeAttackMasksForBothColors();
			bool hasKingOfBothColors = 
				(bitBoardsPiece[QaplaBasics::WHITE_KING] != 0) && (bitBoardsPiece[QaplaBasics::BLACK_KING] != 0);
			bool result = hasKingOfBothColors;
			if (!isWhiteToMove()) {
				result = result && (bitBoardsPiece[QaplaBasics::WHITE_KING] & attackMask[QaplaBasics::BLACK]) == 0;
			}
			else {
				result = result && (bitBoardsPiece[QaplaBasics::BLACK_KING] & attackMask[QaplaBasics::WHITE]) == 0;
			}
			return result;
		}

		/**
		 * Clears/empties the board
		 */
		void clear(); 

		/**
		 * Initializes masks for castling move generator
		 */
		void initCastlingMasksForMoveGeneration();

		/**
		 * Applies a move and updates internal attack masks.
		 * Null moves are handled separately and do not change attack masks.
		 */
		void doMove(QaplaBasics::Move move) {
			if (move.isNullMove()) {
				Board::doNullmove();
				// Attacks are identical after a nullmove
			}
			else {
				Board::doMove(move);
				computeAttackMasksForBothColors();
			}
		}

		/**
		 * Undoes a move and restores the board state.
		 * Handles null moves as well.
		 */
		void undoMove(QaplaBasics::Move move, QaplaBasics::BoardState boardState) {
			if (move.isNullMove()) {
				Board::undoNullmove(boardState);
			}
			else {
				Board::undoMove(move, boardState);
			}
		}

		/**
		 * Sets this board to a mirrored version of the input board (white <-> black).
		 * This can be useful for evaluation symmetry testing or engine self-play balance.
		 */
		void setToSymetricBoard(const MoveGenerator& board) {
			Board::setToSymetricBoard(board);
			computeAttackMasksForBothColors();
		}

		// ------------------------------------------------------------------------
		// ---------------------- Move generation ---------------------------------
		// ------------------------------------------------------------------------

		/*
		* Returns an array of bitboards holding all squares, where pieces can check the king
		*/


		std::array<QaplaBasics::bitBoard_t, QaplaBasics::Piece::PIECE_AMOUNT / 2> computeCheckBitmapsForMovingColor() const;

		/**
		 * Determines if a move results in check. This includes:
		 * - Regular direct checks
		 * - Discovered checks
		 * - Special moves (en passant, castling, promotions)
		 *
		 * Uses the precomputed checkBitmaps from computeCheckBitmaps().
		 */
		bool isCheckMove(QaplaBasics::Move move, const std::array<QaplaBasics::bitBoard_t, 
			QaplaBasics::Piece::PIECE_AMOUNT / 2>& checkingBitmaps);

		/**
		 * Generates all legal moves that evade a check for the side to move.
		 * Includes captures, king moves and interpositions.
		 */
		void genEvadesOfMovingColor(QaplaBasics::MoveList& moveList);

		/**
		 * Generates all moves (silent and non silent) of the color to move
		 */
		void genMovesOfMovingColor(QaplaBasics::MoveList& moveList);

		/**
		 * Generates all non-silent moves for the moving side.
		 * These include captures, promotions, en passant and castling.
		 */
		void genNonSilentMovesOfMovingColor(QaplaBasics::MoveList& moveList);

		/**
		 * Sets a new piece to the board
		 */
		void setPiece(QaplaBasics::Square square, QaplaBasics::Piece piece) {
			Board::setPiece(square, piece);
			computeAttackMasksForBothColors();
		}
		
		/**
		 * Sets a piece, you need to call compute attack masks before move generation
		 */
		void unsafeSetPiece(QaplaBasics::Square square, QaplaBasics::Piece piece) {
			Board::setPiece(square, piece);
		}

		/**
		 * Computes all attack masks for WHITE and BLACK
		 */
		void computeAttackMasksForBothColors();

		/**
		 * Computes the pinned piece mask for the given color.
		 * A pinned piece may only move along the ray between the king and the pinning piece.
		 *
		 * This method works by:
		 * - Identifying all rays from the king to potential blockers.
		 * - Removing pinned-side pieces from the occupancy temporarily.
		 * - Finding all sliding enemy pieces (rooks, bishops, queens) that now attack the king.
		 * - For each such piece, the full ray to the king is added to the pinned mask.
		 *
		 * The result includes both the pinned piece and the allowed movement direction.
		 */
		template <QaplaBasics::Piece COLOR>
		void computePinnedMask();

		/**
		 * @brief provides a SAN for the move
		 * @param move the move
		 */
		std::string moveToSan(QaplaBasics::Move move) const;

		// ------------------------------------------------------------------------
		// ---------------------- Gives check -------------------------------------
		// ------------------------------------------------------------------------

	private:
		enum moveGenType_t { SILENT, NON_SILENT, ALL };

		/**
		 * Generates moves for a single piece on a given square.
		 *
		 * @param piece The piece type to generate for.
		 * @param startPos The square the piece is on.
		 * @param destinationBB Bitboard of legal destination squares.
		 * @param moveList Output list of generated moves.
		 */
		void genMovesSinglePiece(uint32_t piece, QaplaBasics::Square startPos, 
			QaplaBasics::bitBoard_t destinationBB, QaplaBasics::MoveList& moveList);

		/**
		 * Generates moves for all pieces of a certain type that move via a single step pattern,
		 * such as pawns or knights. Often used for mass generation.
		 *
		 * @param piece The piece to move (e.g. WHITE_PAWN).
		 * @param aStep Direction offset for move computation.
		 * @param destinationBB Bitboard of target squares.
		 * @param moveList Output list.
		 */
		void genMovesMultiplePieces(uint32_t piece, int32_t aStep, 
			QaplaBasics::bitBoard_t destinationBB, QaplaBasics::MoveList& moveList);

		/**
		 * Generates en passant move if legal. Handles the edge case where the capture
		 * may cause a discovered check (removing two pieces from a rank).
		 *
		 * The legality is verified by simulating the resulting position and checking
		 * for sliding attacks on the king.
		 */
		template<QaplaBasics::Piece COLOR>
		void genEPMove(QaplaBasics::Square startPos, QaplaBasics::Square epPos, QaplaBasics::MoveList& moveList);

		template<QaplaBasics::Piece COLOR>
		void genPawnPromotions(QaplaBasics::bitBoard_t targetBitBoard, int32_t moveDirection, QaplaBasics::MoveList& moveList);

		template <QaplaBasics::Piece COLOR>
		void genPawnCaptures(QaplaBasics::bitBoard_t targetBitBoard, int32_t moveDirection, QaplaBasics::MoveList& moveList);

		template <QaplaBasics::Piece COLOR>
		void genSilentSinglePawnMoves(QaplaBasics::Square startPos, 
			QaplaBasics::bitBoard_t allowedPositionMask, QaplaBasics::MoveList& moveList);

		template <QaplaBasics::Piece COLOR>
		void genPawnCaptureSinglePiece(QaplaBasics::Square startPos, 
			QaplaBasics::bitBoard_t targetBitBoard, QaplaBasics::MoveList& moveList);

		template <QaplaBasics::Piece COLOR>
		void genSilentPawnMoves(QaplaBasics::MoveList& moveList);

		template <QaplaBasics::Piece COLOR>
		void genNonSilentPawnMoves(QaplaBasics::MoveList& moveList, QaplaBasics::Square epPos);

		template<QaplaBasics::Piece PIECE, moveGenType_t TYPE>
		void genMovesForPiece(QaplaBasics::MoveList& moveList);

		template<MoveGenerator::moveGenType_t TYPE, QaplaBasics::Piece COLOR>
		void genNonPinnedMovesForAllPieces(QaplaBasics::MoveList& moveList);

		template<QaplaBasics::Piece COLOR>
		void genPinnedMovesForAllPieces(QaplaBasics::MoveList& moveList, QaplaBasics::Square epPos);

		template<QaplaBasics::Piece COLOR>
		void genPinnedCapturesForAllPieces(QaplaBasics::MoveList& moveList, QaplaBasics::Square epPos);

		template <QaplaBasics::Piece PIECE>
		void genEvadesByBlocking(QaplaBasics::MoveList& moveList,
			QaplaBasics::bitBoard_t removePinnedPiecesMask,
			QaplaBasics::bitBoard_t blockingPositions);

		/**
		 * Generates all possible moves that evade a check:
		 * - Captures the checking piece
		 * - Interposes along the ray
		 * - Moves the king to safety
		 *
		 * If more than one piece gives check, only king moves are possible.
		 * Handles all special cases including promotions and en passant.
		 */
		template <QaplaBasics::Piece COLOR>
		void genEvades(QaplaBasics::MoveList& moveList);

		template <QaplaBasics::Piece COLOR>
		void genNonSilentMoves(QaplaBasics::MoveList& moveList);

		template <QaplaBasics::Piece COLOR>
		void genMoves(QaplaBasics::MoveList& moveList);

		/**
		 * Computes the attack mask of a single piece (excluding any blocking logic).
		 *
		 * Used for each square individually during attack map generation.
		 * The result is stored in the per-square attack mask array.
		 */
		template<QaplaBasics::Piece PIECE>
		QaplaBasics::bitBoard_t computeAttackMaskForPiece(QaplaBasics::Square square, 
			QaplaBasics::bitBoard_t allPiecesWithoutKing);

		template<QaplaBasics::Piece PIECE>
		QaplaBasics::bitBoard_t computeAttackMaskForPieces(QaplaBasics::bitBoard_t pieceBB, 
			QaplaBasics::bitBoard_t allPiecesWithoutKing);

		template <QaplaBasics::Piece COLOR>
		QaplaBasics::bitBoard_t computeAttackMask();

		template <QaplaBasics::Piece COLOR>
		void computeCastlingMasksForMoveGeneration();

		/**
		 * Computes the set of squares from which each piece type could check the given king.
		 *
		 * This includes:
		 * - Direct check vectors (pawn, knight, rook, bishop, queen)
		 * - Discovered check masks: squares that, if vacated, would expose a check
		 *
		 * These bitboards are used to detect if a move delivers check.
		 */
		template <QaplaBasics::Piece COLOR>
		std::array<QaplaBasics::bitBoard_t, QaplaBasics::Piece::PIECE_AMOUNT / 2> computeCheckBitmaps() const;

		static const int32_t ONE_COLUMN = 1;

	public:

		/**
		 * Bitboards representing all squares attacked by each side.
		 * Indexed by side color: attackMask[WHITE] or attackMask[BLACK].
		 */
		std::array<QaplaBasics::bitBoard_t, 2> attackMask;

		/**
		 * Bitboards marking all pinned pieces for each side.
		 * A pinned piece cannot legally move in arbitrary directions.
		 */
		std::array<QaplaBasics::bitBoard_t, 2> pinnedMask;

		// Squares attacked by pawns
		std::array<QaplaBasics::bitBoard_t, 2> pawnAttack;

		std::array<QaplaBasics::bitBoard_t, QaplaBasics::BOARD_SIZE> pieceAttackMask;
		
		/**
		 * Bitboards used to check if the king passes through attacked squares when castling.
		 * Required for castling legality checks.
		 */
		std::array<QaplaBasics::bitBoard_t, 2> castleAttackMaskKingSide;
		std::array<QaplaBasics::bitBoard_t, 2> castleAttackMaskQueenSide;

		/**
		 * Bitboards used to verify that the castling path is free of pieces.
		 */
		std::array<QaplaBasics::bitBoard_t, 2> castlePieceMaskKingSide;
		std::array<QaplaBasics::bitBoard_t, 2> castlePieceMaskQueenSide;
	};

}
