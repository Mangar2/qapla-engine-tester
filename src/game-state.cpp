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
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include <string>
#include <vector>
#include <cstdint>
#include <tuple>
#include "game-start-position.h"  
#include "../qapla-engine/movegenerator.h"
#include "../qapla-engine/movescanner.h"
#include "../qapla-engine/fenscanner.h"
#include "game-state.h"
#include "logger.h"

namespace QaplaTester {

using namespace QaplaBasics;
using MoveStr = std::string;
using MoveStrList = std::vector<MoveStr>;

GameState::GameState() { 
	setFen(true);
};

bool GameState::setFen(bool startPos, const std::string& fen) {
	bool isValid = true;
	QaplaInterface::FenScanner scanner;
	if (startPos) {
		isValid = scanner.setBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", position_);
	}
	else {
		isValid = scanner.setBoard(fen, position_);
	}
	moveList_.clear();
	boardState_.clear();
	hashList_.clear();
	hashList_.push_back(position_.computeBoardHash());
	gameEndCause_ = GameEndCause::Ongoing;
	gameResult_ = GameResult::Unterminated;
	moveListOutdated = true; 
	if (!isValid) {
		Logger::testLogger().log("GameState::setFen: Invalid FEN string: " + fen, TraceLevel::error);
		position_.clear();
	}
	return isValid;
}

std::tuple<GameEndCause, GameResult> GameState::computeGameResult() {
	if (moveListOutdated) {
		position_.genMovesOfMovingColor(legalMoves_);
		moveListOutdated = false;
	}
	if (legalMoves_.totalMoveAmount == 0) {
		if (position_.isInCheck()) {
			return { GameEndCause::Checkmate, position_.isWhiteToMove() ? 
				GameResult::BlackWins : GameResult::WhiteWins };
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
	const uint32_t reversiblePlies = position_.getHalfmovesWithoutPawnMoveOrCapture();
	uint32_t positionsToCheck = std::min(reversiblePlies, static_cast<uint32_t>(hashList_.size()));
	if (positionsToCheck < 4) {
		return false;
	}

	const auto currentHash = hashList_.back(); // == position_.getZobristHash();
	int repetitions = 1;

	for (uint32_t i = 3; i <= positionsToCheck; i += 2) {
		if (hashList_[hashList_.size() - i] == currentHash) {
			++repetitions;
			if (repetitions >= 3) {
				return true;
			}
		}
	}

	return false;

};

void GameState::doMove(const QaplaBasics::Move& move) {
	moveListOutdated = true; 
	if (move.isEmpty()) {
		return;
	}
	boardState_.push_back(position_.getBoardState());
	position_.doMove(move);
	moveList_.push_back(move);
	hashList_.push_back(position_.computeBoardHash());
}

void GameState::undoMove() {
	moveListOutdated = true;
	if (moveList_.empty()) {
		return;
	}
	position_.undoMove(moveList_.back(), boardState_.back());
	position_.computeAttackMasksForBothColors();
	moveList_.pop_back();
	boardState_.pop_back();
	hashList_.pop_back();
}

void GameState::synchronizeIncrementalFrom(const GameState& referenceState) {
    const auto& refMoves = referenceState.moveList_;
    const auto& ourMoves = moveList_;

    const size_t ourSize = ourMoves.size();
    const size_t refSize = refMoves.size();

    if (ourSize > 0 && refSize > 0 && ourSize <= refSize) {
        if (ourMoves.back() != refMoves[ourSize - 1]) {
            undoMove();
        }
    }

    for (size_t i = ourMoves.size(); i < refSize; ++i) {
        doMove(refMoves[i]);
    }
}

std::string GameState::moveToSan(const QaplaBasics::Move& move) const
{
	if (move.isEmpty()) { 
		return {};
	}
	return position_.moveToSan(move);
}

QaplaBasics::Move GameState::stringToMove(const std::string& moveStr, bool requireLan) 
{
	QaplaInterface::MoveScanner scanner(moveStr);
	if (!scanner.isLegal()) {
		return {};
	}
	if (requireLan && !scanner.isLan()) {
		return {};
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
			(destinationFile == -1 || getFile(move.getDestination()) == File(destinationFile)) &&
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

std::tuple<QaplaBasics::Move, bool, bool> GameState::resolveMove(
	std::optional<Piece> movingPiece,
	std::optional<Square> fromSquare,
	std::optional<Square> toSquare,
	std::optional<Piece> promotionPiece)
{
	QaplaBasics::MoveList moveList;
	QaplaBasics::Move foundMove;
	int matchCount = 0;
	// Promotion is true, if all matching moves are promotions
	bool promotion = true;

	position_.genMovesOfMovingColor(moveList);

	for (uint32_t i = 0; i < moveList.getTotalMoveAmount(); ++i) {
		const Move move = moveList[i];

		if (movingPiece && move.getMovingPiece() != *movingPiece) {
			continue;
		}
		if (fromSquare && getFile(move.getDeparture()) != File(getFile(*fromSquare))) {
			continue;
		}
		if (fromSquare && getRank(move.getDeparture()) != Rank(getRank(*fromSquare))) {
			continue;
		}
		if (toSquare && getFile(move.getDestination()) != File(getFile(*toSquare))) {
			continue;
		}
		if (toSquare && getRank(move.getDestination()) != Rank(getRank(*toSquare))) {
			continue;
		}
		if (move.getPromotion() == NO_PIECE) {
			promotion = false;
		}
		if (promotionPiece && move.getPromotion() != *promotionPiece) {
			continue;
		}
		if (++matchCount == 1) {
			foundMove = move;
		}
	}
	return { matchCount == 1 ? foundMove : QaplaBasics::Move(), matchCount > 0, promotion && matchCount > 0 };
}

GameRecord GameState::setFromGameRecordAndCopy(const GameRecord& game, std::optional<uint32_t> plies, 
	bool verbose) {
	GameRecord copy;
	if (!setFen(game.getStartPos(), game.getStartFen())) {
		return copy;
	}
	copy.setStartPosition(game.getStartPos(), getFen(), isWhiteToMove(), position_.getStartHalfmoves(),
		game.getWhiteEngineName(), game.getBlackEngineName());
	const auto& moves = game.history();
	auto maxPly = static_cast<uint32_t>(moves.size());
	if (plies) {
		maxPly = std::min(maxPly, *plies);
	}
	copy.reserveMoves(maxPly);
	for (uint32_t i = 0; i < maxPly; ++i) {
		MoveRecord move = moves[i];
		auto& moveStr = move.original.empty() ? move.san_ : move.original;
		auto parsed = stringToMove(moveStr, false);
		if (parsed.isEmpty()) {
			if (verbose) {
				Logger::testLogger().log("Illegal move in game record: " + moveStr + " pos: " + getFen(),
					TraceLevel::error);
			}
			return copy;
		}
		move.lan_ = parsed.getLAN();
		move.san_ = moveToSan(parsed);
		copy.addMove(move);
		doMove(parsed);
	}
	uint32_t nextMoveIndex = game.nextMoveIndex();
	copy.setNextMoveIndex(nextMoveIndex);
	auto [gameCause, gameResult] = game.getGameResult();
	copy.setGameEnd(gameCause, gameResult);
	auto [myCause, myResult] = getGameResult();

	if (myResult != GameResult::Unterminated && gameResult == GameResult::Unterminated) {
		copy.setGameEnd(myCause, myResult);
	}
	// Only set the game result if we are at the end of the game record
	if ((!plies || nextMoveIndex == *plies) && myResult == GameResult::Unterminated) {
		setGameResult(gameCause, gameResult);
	}
	return copy;
}

void GameState::setFromGameRecord(const GameRecord& game, std::optional<uint32_t> plies) {
	setFen(game.getStartPos(), game.getStartFen());
	const auto& moves = game.history();
	auto maxPly = static_cast<uint32_t>(moves.size());
	if (plies) {
		maxPly = std::min(maxPly, *plies);
	}
	for (uint32_t i = 0; i < maxPly; ++i) {
		std::string moveStr = moves[i].lan_.empty() ? moves[i].san_ : moves[i].lan_;
		auto parsed = stringToMove(moveStr, false);
		if (parsed.isEmpty()) {
			Logger::testLogger().log("Illegal move in game record: " + moveStr + " pos: " + getFen(),
				TraceLevel::error);
			return;
		}
		doMove(parsed);
	}
	auto [gameCause, gameResult] = game.getGameResult();
	auto [myCause, myResult] = getGameResult();

	// Only set the game result if we are at the end of the game record
	if (game.isGameOver() && myResult == GameResult::Unterminated) {
		setGameResult(gameCause, gameResult);
	} 
}
	
} // namespace QaplaTester
