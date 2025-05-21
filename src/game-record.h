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

 // GameRecord.h
#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "move-record.h"
#include "time-control.h"
#include "game-result.h"

/**
 * Stores a list of moves and manages current game state pointer.
 * Supports forward/backward navigation and time control evaluation.
 */
class GameRecord {
public:
    void newGame(bool startPos, std::string startFen, bool isWhiteToMove) {
		moves_.clear();
		isWhiteToMove_ = isWhiteToMove;
		currentPly_ = 0;
		startPos_ = startPos;
		startFen_ = startFen;
		gameEndCause_ = GameEndCause::Ongoing;
		gameResult_ = GameResult::Unterminated;
    }
    /** Adds a move at the current ply position, overwriting any future moves. */
    void addMove(const MoveRecord& move);

    /** Returns the current ply index. */
    uint32_t currentPly() const;

    /** Sets the current ply (0 = before first move). */
    void setPly(uint32_t ply);

    /** Advances to the next ply if possible. */
    void advance();

    /** Rewinds to the previous ply if possible. */
    void rewind();

    /**
     * Returns the total time used by each side up to the current ply.
     *
     * @return A pair of milliseconds used: {whiteTime, blackTime}
     */
    std::pair<uint64_t, uint64_t> timeUsed() const;

    /** Returns const reference to move history. */
    const std::vector<MoveRecord>& history() const;

	bool getStartPos() const { return startPos_; }
	std::string getStartFen() const { return startFen_; }

    /** 
	 * @brief Sets the game end cause and result.
	 * @param cause The cause of the game end.
	 * @param result The result of the game.
     */
	void setGameEnd(GameEndCause cause, GameResult result) {
		gameEndCause_ = cause;
		gameResult_ = result;
	}
	/**
	 * @brief Returns the game end cause and result.
	 * @return A pair of GameEndCause and GameResult.
	 */
	std::tuple<GameEndCause, GameResult> getGameResult() const {
		return { gameEndCause_, gameResult_ };
	}
    /**
	 * @brief Sets the time control for the game.
     */
    void setTimeControl(const TimeControl& whiteTimeControl, const TimeControl& blackTimeControl) {
        whiteTimeControl_ = whiteTimeControl;
		blackTimeControl_ = blackTimeControl;
    }
    /**
     * @brief Returns the white side's time control.
     */
    const TimeControl& getWhiteTimeControl() const {
        return whiteTimeControl_;
    }

    /**
     * @brief Returns the black side's time control.
     */
    const TimeControl& getBlackTimeControl() const {
        return blackTimeControl_;
    }

	/**
	 * @brief Returns the current side to move.
	 */
	const bool isWhiteToMove() const {
		return isWhiteToMove_;
	}
    

private:
    bool startPos_ = true;
    std::string startFen_;
    std::vector<MoveRecord> moves_;
    uint32_t currentPly_ = 0;
    GameEndCause gameEndCause_;
	GameResult gameResult_;
    TimeControl whiteTimeControl_;
    TimeControl blackTimeControl_;
	bool isWhiteToMove_ = true;
};