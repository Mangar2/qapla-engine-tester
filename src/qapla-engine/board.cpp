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

#include <iomanip>
#include "bits.h"
#include "board.h"
#include "bitboardmasks.h"
#include "pst.h"

using namespace QaplaBasics;

Board::Board() {
	clear();
	initClearCastleMask();
}

void Board::clear() {
	clearBB();
	_pieceSignature.clear();
	_materialBalance.clear();
	_pstBonus = 0;
	// Important to keep legal positions for the king squares even on a clear board
	kingSquares[WHITE] = E1;
	kingSquares[BLACK] = E8;
	_kingStartSquare = { E1, E8 };
	_queenRookStartSquare = { A1, A8 };
	_kingRookStartSquare = { H1, H8 };
	_startHalfmoves = 0;
	_boardState.initialize();
	_board.fill(NO_PIECE);
}

void Board::initClearCastleMask() {
	_clearCastleFlagMask.fill(0xFFFF);
	_clearCastleFlagMask[_queenRookStartSquare[WHITE]] = static_cast<uint16_t>(~BoardState::WHITE_QUEEN_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_kingRookStartSquare[WHITE]] = static_cast<uint16_t>(~BoardState::WHITE_KING_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_queenRookStartSquare[BLACK]] = static_cast<uint16_t>(~BoardState::BLACK_QUEEN_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_kingRookStartSquare[BLACK]] = static_cast<uint16_t>(~BoardState::BLACK_KING_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_kingStartSquare[WHITE]] =
		static_cast<uint16_t>(~(BoardState::WHITE_QUEEN_SIDE_CASTLE_BIT + BoardState::WHITE_KING_SIDE_CASTLE_BIT));
	_clearCastleFlagMask[_kingStartSquare[BLACK]] =
		static_cast<uint16_t>(~(BoardState::BLACK_QUEEN_SIDE_CASTLE_BIT + BoardState::BLACK_KING_SIDE_CASTLE_BIT));
}

void Board::setToSymetricBoard(const Board& board) {
	clear();
	for (Square square = A1; square <= H8; ++square) {
		const Piece piece = board[square];
		if (piece != NO_PIECE) {
			setPiece(Square(square ^ 0x38), Piece(piece ^ 1));
		}
	}
	setCastlingRight(WHITE, true, isKingSideCastleAllowed<WHITE>());
	setCastlingRight(WHITE, false, isQueenSideCastleAllowed<WHITE>());
	setCastlingRight(BLACK, true, isKingSideCastleAllowed<BLACK>());
	setCastlingRight(BLACK, false, isQueenSideCastleAllowed<BLACK>());
	setEP(Square(getEP() ^ 0x38));
	setWhiteToMove(!board.isWhiteToMove());
}

void Board::removePiece(Square square) {
	Piece pieceToRemove = _board[square];
	removePieceBB(square, pieceToRemove);
	_boardState.updateHash(square, pieceToRemove);
	_board[square] = NO_PIECE;
	_pieceSignature.removePiece(pieceToRemove, bitBoardsPiece[pieceToRemove]);
	_materialBalance.removePiece(pieceToRemove);
	_pstBonus -= PST::getValue(square, pieceToRemove);
}

void Board::addPiece(Square square, Piece pieceToAdd) {
	_pieceSignature.addPiece(pieceToAdd);
	addPieceBB(square, pieceToAdd);
	_boardState.updateHash(square, pieceToAdd);
	_board[square] = pieceToAdd;
	_materialBalance.addPiece(pieceToAdd);
	_pstBonus += PST::getValue(square, pieceToAdd);
}

void Board::movePiece(Square fromSquare, Square toSquare) {
	Piece pieceToMove = _board[fromSquare];
	if (isKing(pieceToMove)) {
		kingSquares[getPieceColor(pieceToMove)] = toSquare;
	}
	_pstBonus += PST::getValue(toSquare, pieceToMove) -
		PST::getValue(fromSquare, pieceToMove);
	movePieceBB(fromSquare, toSquare, pieceToMove);
	_boardState.updateHash(fromSquare, pieceToMove);
	_board[fromSquare] = NO_PIECE;
	_boardState.updateHash(toSquare, pieceToMove);
	_board[toSquare] = pieceToMove;
}

void Board::doMoveSpecialities(Move move) {
	
	Square destination = move.getDestination();
	switch (move.getActionAndMovingPiece())
	{
	case Move::WHITE_PROMOTE:
	case Move::BLACK_PROMOTE:
		removePiece(destination);
		addPiece(destination, move.getPromotion());
		break;
	case Move::WHITE_EP:
		removePiece(destination + SOUTH);
		break;
	case Move::BLACK_EP:
		removePiece(destination + NORTH);
		break;
	case Move::WHITE_CASTLES_KING_SIDE:
		if (_kingRookStartSquare[WHITE] != F1) {
			movePiece(_kingRookStartSquare[WHITE], F1);
		}
		break;
	case Move::WHITE_CASTLES_QUEEN_SIDE:
		if (_queenRookStartSquare[WHITE] != D1) {
			movePiece(_queenRookStartSquare[WHITE], D1);
		}
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		if (_kingRookStartSquare[BLACK] != F8) {
			movePiece(_kingRookStartSquare[BLACK], F8);
		}
		break;
	case Move::BLACK_CASTLES_QUEEN_SIDE:
		if (_queenRookStartSquare[BLACK] != D8) {
			movePiece(_queenRookStartSquare[BLACK], D8);
		}
		break;
	default:
		// Nothing to do
		break;
	}
}

void Board::doMove(Move move) {
	assert(assertMove(move));

	Square departure = move.getDeparture();
	Square destination = move.getDestination();
	updateStateOnDoMove(departure, destination);

	if (move.isCaptureMoveButNotEP())
	{
		removePiece(destination);
	}
	movePiece(departure, destination);

	if (move.getAction() != 0) {
		doMoveSpecialities(move);
	}

	assert(_board[departure] == NO_PIECE || move.isCastleMove());
	assert(_board[destination] != NO_PIECE);

}


/**
 * Update all based for doMove
 * @param departure departure position of the move
 * @param destination destination position of the move
 */
void Board::updateStateOnDoMove(Square departure, Square destination) {
	_whiteToMove = !_whiteToMove;
	_boardState.clearEP();
	_boardState.disableCastlingRightsByMask(
		_clearCastleFlagMask[departure] & _clearCastleFlagMask[destination]);
	_boardState.halfmovesWithoutPawnMoveOrCapture++;
	bool isCapture = _board[destination] != NO_PIECE;
	bool isPawnMove = isPawn(_board[departure]);
	bool isMoveTwoRanks = ((departure - destination) & 0x0F) == 0;
	if (isCapture || isPawnMove) {
		_boardState.halfmovesWithoutPawnMoveOrCapture = 0;
		_boardState.fenHalfmovesWithoutPawnMoveOrCapture = 0;
	}
	if (isPawnMove && isMoveTwoRanks) {
		_boardState.setEP(destination);
	}
}

void Board::undoMoveSpecialities(Move move) {
	Square destination = move.getDestination();

	switch (move.getActionAndMovingPiece())
	{
	case Move::WHITE_PROMOTE:
		removePiece(destination);
		addPiece(destination, WHITE_PAWN);
		break;
	case Move::BLACK_PROMOTE:
		removePiece(destination);
		addPiece(destination, BLACK_PAWN);
		break;
	case Move::WHITE_EP:
		addPiece(destination + SOUTH, BLACK_PAWN);
		break;
	case Move::BLACK_EP:
		addPiece(destination + NORTH, WHITE_PAWN);
		break;
	case Move::WHITE_CASTLES_KING_SIDE:
		assert(_board[G1] == WHITE_KING);
		removePiece(G1);
		if (_kingRookStartSquare[WHITE] != F1) {
			movePiece(F1, _kingRookStartSquare[WHITE]);
		}
		addPiece(_kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = _kingStartSquare[WHITE];
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		assert(_board[G8] == BLACK_KING);
		removePiece(G8);
		if (_kingRookStartSquare[BLACK] != F8) {
			movePiece(F8, _kingRookStartSquare[BLACK]);
		}
		addPiece(_kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = _kingStartSquare[BLACK];
		break;

	case Move::WHITE_CASTLES_QUEEN_SIDE:
		assert(_board[C1] == WHITE_KING);
		removePiece(C1);
		if (_queenRookStartSquare[WHITE] != D1) {
			movePiece(D1, _queenRookStartSquare[WHITE]);
		}
		addPiece(_kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = _kingStartSquare[WHITE];
		break;

	case Move::BLACK_CASTLES_QUEEN_SIDE:
		assert(_board[C8] == BLACK_KING);
		removePiece(C8);
		if (_queenRookStartSquare[BLACK] != D8) {
			movePiece(D8, _queenRookStartSquare[BLACK]);
		}
		addPiece(_kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = _kingStartSquare[BLACK];
		break;
	default:
		// Nothing to do
		break;
	}

}

void Board::undoMove(Move move, BoardState recentBoardState) {

	Square departure = move.getDeparture();
	Square destination = move.getDestination();
	Piece capture = move.getCapture();
	if (move.getAction() != 0) {
		undoMoveSpecialities(move);
	} 

	if (!move.isCastleMove())
	{
		assert(_board[destination] == move.getMovingPiece());
		movePiece(destination, departure);
		if (move.isCaptureMoveButNotEP()) {
			addPiece(destination, capture);
		}
	}
	_whiteToMove = !_whiteToMove;
	_boardState = recentBoardState;
	assert(_board[departure] != NO_PIECE);
}

std::string Board::getFen(uint32_t halfmovesPlayed) const {
	std::string result;
	File file;
	Rank rank;
	int amoutOfEmptyFields;
	for (rank = Rank::R8; rank >= Rank::R1; --rank)
	{
		amoutOfEmptyFields = 0;
		for (file = File::A; file <= File::H; ++file)
		{
			Square square = computeSquare(file, rank);
			Piece piece = operator[](square);
			if (piece == Piece::NO_PIECE) {
				amoutOfEmptyFields++;
			} else {
				if (amoutOfEmptyFields > 0) {
					result += std::to_string(amoutOfEmptyFields);
				}
				result.push_back(pieceToChar(piece));
				amoutOfEmptyFields = 0;
			}
		}
		if (amoutOfEmptyFields > 0) {
			result += std::to_string(amoutOfEmptyFields);
		}
		if (rank > Rank::R1) {
			result.push_back('/');
		}
	}

	result += isWhiteToMove()? " w" : " b";

	result += " ";
	std::string castling;
	castling += getBoardState().isKingSideCastleAllowed<WHITE>() ? "K" : "";
	castling += getBoardState().isQueenSideCastleAllowed<WHITE>() ? "Q" : "";
	castling += getBoardState().isKingSideCastleAllowed<BLACK>() ? "k" : "";
	castling += getBoardState().isQueenSideCastleAllowed<BLACK>() ? "q" : "";
	result += castling.empty() ? "-" : castling;

	result += " ";
	auto uncorrectedEp = getBoardState().getEP();
	// adjust for the fact that we store the square of the pawn to be captured
	auto correctedEp = getRank(uncorrectedEp) == Rank::R4 ? uncorrectedEp + SOUTH : uncorrectedEp + NORTH;
	result += getBoardState().hasEP() ? squareToString(correctedEp) : "-";

	result += " ";
	result += std::to_string(getTotalHalfmovesWithoutPawnMoveOrCapture());
	halfmovesPlayed = std::max<uint32_t>(_startHalfmoves, halfmovesPlayed);
	int fullMoveNumber = (halfmovesPlayed) / 2 + 1;
	result += " ";
	result += std::to_string(fullMoveNumber);

	return result;
}

void Board::printPst(Piece piece) const {
	auto pieceBB = getPieceBB(piece);
	if (pieceBB == 0) {
		return;
	}

	std::cout << " " << colorToString(getPieceColor(piece)) 
		<< " " << pieceToChar(piece) << " PST: " 
		<< std::right << std::setw(19);

	EvalValue total;

	for (auto bb = pieceBB; bb != 0; bb &= bb - 1)
	{
		const auto square = lsb(bb);
		total += PST::getValue(square, piece);
	}
	std::cout << total << " (";

	for (auto bb = pieceBB; bb != 0; bb &= bb - 1)
	{
		const auto square = lsb(bb);
		const auto value = PST::getValue(square, piece);
		std::cout << squareToString(square) << value << " ";
	}
	std::cout << ")\n" << std::flush;
}


void Board::printPst() const {
	for (Piece piece = MIN_PIECE; piece <= BLACK_KING; ++piece) {
		printPst(piece);
	}
}


void Board::printFen() const {
	std::cout << getFen() << "\n" << std::flush;
}

void Board::print() const {
	for (Rank rank = Rank::R8; rank >= Rank::R1; --rank) {
		for (File file = File::A; file <= File::H; ++file) {
			Piece piece = operator[](computeSquare(file, rank));
			std::cout << " " << pieceToChar(piece) << " ";
		}
		std::cout << "\n";
	}
	std::cout << "hash: " << computeBoardHash() << "\n";
	printFen();
	//printf("White King: %ld, Black King: %ld\n", kingPos[WHITE], kingPos[BLACK]);
}

bool Board::assertMove(Move move) const {
	assert(move.getMovingPiece() != NO_PIECE);
	assert(move.getDeparture() != move.getDestination());
	if (!(move.getMovingPiece() == operator[](move.getDeparture()))) {
		move.print();
	}
	assert(move.getMovingPiece() == operator[](move.getDeparture()));
	assert((move.getCapture() == operator[](move.getDestination())) || move.isCastleMove() || move.isEPMove());
	return true;
}

bool Board::isValidPosition() const {
	
	if (!validatePieceCounts()) {
		return false;
	}
	if (!validatePawnRows()) {
		return false;
	}
	if (!validateEPSquare()) {
		return false;
	}
	if (!validateCastlingRights()) {
		return false;
	}
	return true;
}

bool Board::validatePawnRows() const {
	// Check pawns not on 1st or 8th rank
	const bitBoard_t whitePawnBB = bitBoardsPiece[WHITE_PAWN];
	const bitBoard_t blackPawnBB = bitBoardsPiece[BLACK_PAWN];
	const bitBoard_t pawnMask = QaplaMoveGenerator::BitBoardMasks::RANK_1_BITMASK | QaplaMoveGenerator::BitBoardMasks::RANK_8_BITMASK;
	if ((whitePawnBB & pawnMask) != 0) {
		return false;
	}
	if ((blackPawnBB & pawnMask) != 0) {
		return false;
	}
	return true;
}

bool Board::validatePieceCounts() const {
	// Check exactly one white and one black king
	if (popCount(bitBoardsPiece[WHITE_KING]) != 1 || popCount(bitBoardsPiece[BLACK_KING]) != 1) {
		return false;
	}
	uint32_t whitePawns = popCount(bitBoardsPiece[WHITE_PAWN]);
	uint32_t blackPawns = popCount(bitBoardsPiece[BLACK_PAWN]);

	// Check pawn limits
	if (whitePawns > 8 || blackPawns > 8) {
		return false;
	}
	// Check piece limits
	for (Piece piece = WHITE_KNIGHT; piece <= BLACK_BISHOP; ++piece) {
		uint32_t count = popCount(bitBoardsPiece[piece]);
		count += getPieceColor(piece) == WHITE ? whitePawns : blackPawns;
		if (count > 10) { 
			return false;
		}
	}
	return true;
}

bool Board::validateEPSquare() const {
	const Square ep = getEP();
	if (ep == 0) {
		return true;
	}
	const Rank epRank = getRank(ep);
	// Internal representation: EP square is the square of the pawn moved two squares (different from FEN)
	auto requiredRank = _whiteToMove ? Rank::R5 : Rank::R4;
	// if White to move, last move was by Black.
	auto requiredPawn = _whiteToMove ? BLACK_PAWN : WHITE_PAWN;
	return (epRank == requiredRank && _board[ep] == requiredPawn);
}

bool Board::validateCastlingRights() const {
	// Check castling rights
	if (isKingSideCastleAllowed<WHITE>()) {
		if (_board[_kingRookStartSquare[WHITE]] != WHITE_ROOK || kingSquares[WHITE] != _kingStartSquare[WHITE]) {
			return false;
		}
	}
	if (isQueenSideCastleAllowed<WHITE>()) {
		if (_board[_queenRookStartSquare[WHITE]] != WHITE_ROOK || kingSquares[WHITE] != _kingStartSquare[WHITE]) {
			return false;
		}
	}
	if (isKingSideCastleAllowed<BLACK>()) {
		if (_board[_kingRookStartSquare[BLACK]] != BLACK_ROOK || kingSquares[BLACK] != _kingStartSquare[BLACK]) {
			return false;
		}
	}
	if (isQueenSideCastleAllowed<BLACK>()) {
		if (_board[_queenRookStartSquare[BLACK]] != BLACK_ROOK || kingSquares[BLACK] != _kingStartSquare[BLACK]) {
			return false;
		}
	}
	return true;
}

void Board::setupAddPiece(Square square, Piece piece) {
	if (square == NO_SQUARE || piece == NO_PIECE) {
		return;
	}
	addPiece(square, piece);
	auto pieceType = getPieceType(piece);
	auto pieceColor = getPieceColor(piece);
	// auto enabling castling rights if king or rook is placed on its starting square
	if (pieceType == KING) {
		if (kingSquares[pieceColor] != square && _board[kingSquares[pieceColor]] == piece) {
			setupRemovePiece(kingSquares[pieceColor]);
		}
		kingSquares[pieceColor] = square;
		if (square == _kingStartSquare[pieceColor]) {
			if (_board[_kingRookStartSquare[pieceColor]] == piece) {
				setCastlingRight(pieceColor, true, true);
			} 
			if (_board[_queenRookStartSquare[pieceColor]] == piece) {
				setCastlingRight(pieceColor, false, true);
			}
		} 
	}
	if (pieceType == ROOK) {
		if (square == _kingRookStartSquare[pieceColor]) {
			if (kingSquares[pieceColor] == _kingStartSquare[pieceColor]) {
				setCastlingRight(pieceColor, true, true);
			}
		} 
		if (square == _queenRookStartSquare[pieceColor]) {
			if (kingSquares[pieceColor] == _kingStartSquare[pieceColor]) {
				setCastlingRight(pieceColor, false, true);
			}
		} 
	}
}

void Board::setupRemovePiece(Square square) {
	if (square == NO_SQUARE) {
		return;
	}
	auto piece = _board[square];
	if (piece == NO_PIECE) {
		return;
	}
	auto pieceType = getPieceType(piece);
	auto pieceColor = getPieceColor(piece);
	bool isPawn = pieceType == PAWN;
	removePiece(square);
	if (kingSquares[pieceColor] == square) {
		kingSquares[pieceColor] = NO_SQUARE;
	}
	_boardState.disableCastlingRightsByMask(_clearCastleFlagMask[square]);
	if (isPawn && square == _boardState.getEP()) {
		_boardState.clearEP();
	}
}

Square Board::getSetupEpSquare() const {
	auto internalEpSquare = _boardState.getEP();
	if (internalEpSquare == 0) {
		return NO_SQUARE;
	}
	auto epRank = getRank(internalEpSquare);
	if (epRank == Rank::R4) {
		return Square(internalEpSquare + SOUTH);
	}
	if (epRank == Rank::R5) {
		return Square(internalEpSquare + NORTH);
	}
	return NO_SQUARE;
}

void Board::setSetupEpSquare(Square epSquare) {
	if (epSquare == NO_SQUARE) {
		_boardState.setEP(Square(0));
		return;
	}
	auto epRank = getRank(epSquare);
	if (epRank == Rank::R3) {
		_boardState.setEP(Square(epSquare + NORTH));
		return;
	}
	if (epRank == Rank::R6) {
		_boardState.setEP(Square(epSquare + SOUTH));
		return;
	}
	_boardState.setEP(Square(0));
}