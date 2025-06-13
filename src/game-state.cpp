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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#include <string>
#include <vector>
#include <cstdint>
#include <tuple>
#include "game-start-position.h"  // enth�lt GameType + FEN
#include "movegenerator.h"
#include "movescanner.h"
#include "fenscanner.h"
#include "game-state.h"

using MoveStr = std::string;
using MoveStrList = std::vector<MoveStr>;

GameState::GameState() { 
	setFen(true);
};

void GameState::setFen(bool startPos, const std::string fen) {
	QaplaInterface::FenScanner scanner;
	if (startPos) {
		scanner.setBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", position_);
	}
	else {
		scanner.setBoard(fen, position_);
	}
	moveList_.clear();
	boardState_.clear();
	hashList_.clear();
	hashList_.push_back(position_.computeBoardHash());
	gameEndCause_ = GameEndCause::Ongoing;
	gameResult_ = GameResult::Unterminated;
	moveListOutdated = true; 
}

std::tuple<GameEndCause, GameResult> GameState::computeGameResult() {
	if (moveListOutdated) {
		position_.genMovesOfMovingColor(legalMoves_);
		moveListOutdated = false;
	}
	if (legalMoves_.totalMoveAmount == 0) {
		if (position_.isInCheck()) {
			if (position_.isWhiteToMove()) {
				return { GameEndCause::Checkmate, GameResult::BlackWins };
			}
			else {
				return { GameEndCause::Checkmate, GameResult::WhiteWins };
			}
		}
		return { GameEndCause::Stalemate, GameResult::Draw };
	}
	if (position_.drawDueToMissingMaterial()) {
		return { GameEndCause::DrawByInsufficientMaterial, GameResult::Draw };
	}
	if (position_.getHalfmovesWithoutPawnMoveOrCapture() >= 100) {
		return { GameEndCause::DrawByFiftyMoveRule, GameResult::Draw };
	}

	if (isThreefoldRepetition()) {
		return { GameEndCause::DrawByRepetition, GameResult::Draw };
	}

	return { GameEndCause::Ongoing, GameResult::Unterminated };
}


std::tuple<GameEndCause, GameResult> GameState::getGameResult() {
	if (gameResult_ != GameResult::Unterminated) {
		return { gameEndCause_, gameResult_ };
	}
	auto [cause, result] = computeGameResult();
	gameEndCause_ = cause;
	gameResult_ = result;
	return { gameEndCause_, gameResult_ };
}

bool GameState::isThreefoldRepetition() const {
	const uint16_t reversiblePlies = position_.getHalfmovesWithoutPawnMoveOrCapture();
	uint32_t positionsToCheck = std::min(reversiblePlies, static_cast<uint16_t>(hashList_.size()));
	if (positionsToCheck < 4) return false;

	const auto currentHash = hashList_.back(); // == position_.getZobristHash();
	int repetitions = 1;

	for (uint32_t i = 3; i <= positionsToCheck; i += 2) {
		if (hashList_[hashList_.size() - i] == currentHash) {
			++repetitions;
			if (repetitions >= 3) return true;
		}
	}

	return false;

};

void GameState::doMove(const QaplaBasics::Move& move) {
	moveListOutdated = true; 
	if (move.isEmpty()) return;
	boardState_.push_back(position_.getBoardState());
	position_.doMove(move);
	moveList_.push_back(move);
	hashList_.push_back(position_.computeBoardHash());
}

void GameState::undoMove() {
	moveListOutdated = true;
	if (moveList_.empty()) return;
	position_.undoMove(moveList_.back(), boardState_.back());
	position_.computeAttackMasksForBothColors();
	moveList_.pop_back();
	boardState_.pop_back();
	hashList_.pop_back();
}

std::string GameState::moveToSan(const QaplaBasics::Move& move) const
{
	if (move.isEmpty()) return std::string();
	return position_.moveToSan(move);
}

QaplaBasics::Move GameState::stringToMove(std::string move, bool requireLan) 
{
	QaplaInterface::MoveScanner scanner(move);
	if (!scanner.isLegal()) {
		return QaplaBasics::Move::EMPTY_MOVE;
	}
	if (requireLan && !scanner.isLan()) {
		return QaplaBasics::Move::EMPTY_MOVE;
	}
	int32_t departureFile = scanner.departureFile;
	int32_t departureRank = scanner.departureRank;
	int32_t destinationFile = scanner.destinationFile;
	int32_t destinationRank = scanner.destinationRank;
	char promotePieceChar = scanner.promote;
	char movingPieceChar = scanner.piece;

	QaplaBasics::MoveList moveList;
	QaplaBasics::Move foundMove;

	position_.genMovesOfMovingColor(moveList);
	const bool whiteToMove = position_.isWhiteToMove();
	Piece promotePiece = charToPiece(whiteToMove ? toupper(promotePieceChar) : tolower(promotePieceChar));
	Piece movingPiece = charToPiece(whiteToMove ? toupper(movingPieceChar) : tolower(movingPieceChar));

	for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
		const QaplaBasics::Move move = moveList[moveNo];

		if ((movingPiece == NO_PIECE || move.getMovingPiece() == movingPiece) &&
			(departureFile == -1 || getFile(move.getDeparture()) == File(departureFile)) &&
			(departureRank == -1 || getRank(move.getDeparture()) == Rank(departureRank)) &&
			(getFile(move.getDestination()) == File(destinationFile)) &&
			(destinationRank == -1 || getRank(move.getDestination()) == Rank(destinationRank)) &&
			(move.getPromotion() == promotePiece))
		{
			if (!foundMove.isEmpty()) {
				// Error: more than one possible move found
				foundMove = QaplaBasics::Move();
				break;
			}
			foundMove = move;
		}
	}

	return foundMove;
}
	
