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
#pragma once
#include <atomic>
#include "engine-worker.h"
#include "time-control.h"
#include "game-state.h"
#include "game-record.h"
#include "engine-report.h"

class PlayerContext {
public:
    PlayerContext() = default;

    /**
     * @brief Sets the time control for this player.
     *
     * @param timeControl The time control to apply.
     */
    void setTimeControl(const TimeControl& timeControl) {
        timeControl_ = timeControl;
    }

    /**
     * @brief Sets the engine worker for this player.
     *
     * @param engineWorker Shared pointer to the EngineWorker.
     */
    void setEngine(std::unique_ptr<EngineWorker> engineWorker, bool requireLan) {
        computingMove_ = false;
		if (!engineWorker) {
			throw AppError::makeInvalidParameters("Cannot set a null engine worker");
		}
		checklist_ = EngineReport::getChecklist(engineWorker->getConfig().getName());
        engine_ = std::move(engineWorker);

		requireLan_ = requireLan;
    }

    /**
     * @brief Returns a raw pointer to the EngineWorker instance.
     *
     * @return Pointer to the EngineWorker.
     */
    EngineWorker* getEngine() {
        return engine_.get();
    }

	/**
	 * @brief Returns the identifier of the engine.
     */
	std::string getIdentifier() const {
		if (engine_) {
			return engine_->getIdentifier();
		}
		return "";
	}

    /**
	 * @brief Informs the engine that a new game is starting.
     */
    void notifyNewGame() {
        if (engine_) {
            engine_->newGame();
        }
    }

    /**
	 * @brief Tells the engine to compute a new move
     */
    void computeMove(const GameRecord& gameRecord, const GoLimits& goLimits);

    /**
	 * @brief Allows the engine to ponder during its turn.
	 * @param gameRecord The current game with startposition and moves played so far.
	 * @param goLimits The time limits for the next move
	 * @param event the event that triggered the pondering, if any
     */
    void allowPonder(const GameRecord& gameRecord, const GoLimits& goLimits, 
        const std::optional<EngineEvent>& event);

	/**
	 * @brief Cancels the current move computation.
	 *
	 * This function requests the engine to cancel computing the current move.
	 * If the engine responds with a move, it will be ignored.
	 */
    void cancelCompute() {
        if (!engine_) return;
        constexpr auto readyTimeout = std::chrono::seconds{ 1 };
        if (computingMove_ || (pondering_ && !ponderMove_.empty())) {
            engine_->moveNow(true);
            engine_->requestReady(readyTimeout);
        }
        computingMove_ = false;
        pondering_ = false;
        ponderMove_ = "";
    }

	/**
	 * @brief returns player to move
	 */
	bool isWhiteToMove() const {
		return gameState_.isWhiteToMove();
	}

	/**
	 * @brief Returns the current move record.
	 */
	const MoveRecord& getCurrentMove() const {
		return currentMove_;
	}

	/**
	 * @brief Returns the current game state.
     * @return The result of the game and the winner side.
     */
	auto getGameResult() {
		return gameState_.getGameResult();
	}

    /**
     * @brief Sets the timestamp when the engine started computing a move.
     * We will never reduce the start timestamp. If a new timestamp is lower than the last one
     * we obviously have a race condition and thus ignore the too-old timestamp.
     * @param timestamp Milliseconds since epoch.
     */
    void setComputeMoveStartTimestamp(int64_t timestamp) {
        if (timestamp > computeMoveStartTimestamp_) {
            computeMoveStartTimestamp_ = timestamp;
        }
        // I expect this to never happen. Let us see in debug mode. If this happens this should be a race condition.
        // But it still make sense to check, if this is a bug in the code
        assert(timestamp >= computeMoveStartTimestamp_);
    }

    /**
     * @brief Gets the timestamp when the engine started computing a move.
     *
     * @return Timestamp in milliseconds since epoch.
     */
    int64_t getComputeMoveStartTimestamp() const {
        return computeMoveStartTimestamp_;
    }

    /**
     * @brief Handles an info event from the engine.
     *
     * @param event The EngineEvent containing the information.
     */
    void handleInfo(const EngineEvent& event);

	/**
	 * @brief Keep alive tick - check for a timout or non active engine
     * @return true, if we restarted the engine and the task must be stopped
	 */
    bool checkEngineTimeout();

    /**
     * @brief Handles a best move event from the engine.
     *
     * @param event The EngineEvent containing the best move.
	 * @return The best move as a QaplaBasics::Move object.
     */
    QaplaBasics::Move handleBestMove(const EngineEvent& event);

	/**
	 * @brief Handles a disconnect event.
	 *
	 * This function is called when the engine unexpectedly disconnects.
	 * It sets the game result to indicate a disconnection and restarts the engine.
	 *
	 * @param isWhitePlayer True if this player is the white player, false if black.
	 */
    void handleDisconnect(bool isWhitePlayer);

    /**
     * @brief Evaluates whether the engine respected the time constraints.
     *
     * This function checks if the engine respected active time constraints like movetime,
     * remaining time, and increment. It also verifies that the engine used time appropriately
     * even if no strict constraints were set.
     *
     * @param event The EngineEvent containing timing information.
     */
    void checkTime(const EngineEvent& event);

    /**
	 * @brief Plays a move in the game.
     *
     * @param move The move.
     */
    void doMove(QaplaBasics::Move move);
    
    /**
	 * @brief Sets the game state from a GameRecord.
	 * @param startPosition The GameRecord to set the game state from.
     */
    void setStartPosition(const GameRecord& startPosition) {
        gameState_.setFromGameRecord(startPosition);
    }

    /**
     * @brief Sets the game state to a new position.
     *
     * @param startPosition If true, sets the game to the starting position.
     * @param fen The FEN string representing the new position.
     */
    void setStartPosition(bool startPosition, const std::string& fen) {
        gameState_.setFen(startPosition, fen);
    }

	const TimeControl& getTimeControl() const {
		return timeControl_;
	}

private:
    /**
     * @brief Checks if a move is legal in the current game state.
     *
     * @param moveText The move in algebraic notation.
     * @return true if the move is legal, false otherwise.
     */
    bool isLegalMove(const std::string& moveText);

    bool restartIfNotReady();
    void restart();

    std::unique_ptr<EngineWorker> engine_;
    TimeControl timeControl_;
    GameState gameState_;
    int64_t computeMoveStartTimestamp_ = 0;
    GoLimits goLimits_;
    bool requireLan_;
	bool pondering_ = false;
	std::string ponderMove_ = "";
    std::atomic<bool> computingMove_ = false;
    MoveRecord currentMove_;
	EngineReport* checklist_ = nullptr; 
};
