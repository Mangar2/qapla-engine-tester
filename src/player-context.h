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
#pragma once
#include <atomic>
#include "engine-worker.h"
#include "time-control.h"
#include "game-state.h"
#include "game-record.h"
#include "engine-report.h"

namespace QaplaTester {

inline thread_local bool isEventQueueThread = false;

class PlayerContext {
public:
    PlayerContext() = default;
    PlayerContext(PlayerContext&&) = delete;
    PlayerContext& operator=(PlayerContext&&) = delete;

    PlayerContext(const PlayerContext&) = delete;
    PlayerContext& operator=(const PlayerContext&) = delete;

    /**
     * @brief Sets the time control for this player.
     *
     * @param gameRecord The current game state including time controls.
     * @param engineIsWhite True if the engine plays as white, false for black.
     */
    void setTimeControl(const GameRecord& gameRecord, bool engineIsWhite) {
        timeControl_ = engineIsWhite ? gameRecord.getWhiteTimeControl() : gameRecord.getBlackTimeControl();
        if (engine_) {
            engine_->setTimeControl(gameRecord, engineIsWhite);
        }
    }

    /**
     * @brief Sets the engine worker for this player.
     *
     * @param engineWorker Shared pointer to the EngineWorker.
     */
    void setEngine(std::unique_ptr<EngineWorker> engineWorker) {
        computeState_ = ComputeState::Idle;
		if (!engineWorker) {
			throw AppError::makeInvalidParameters("Cannot set a null engine worker");
		}
		checklist_ = EngineReport::getChecklist(engineWorker->getConfig().getName());
        engine_ = std::move(engineWorker);
		requireLan_ = engine_->getConfig().getProtocol() == EngineProtocol::Uci;
    }

    /**
     * @brief Resets the engine worker and its state.
     *
     * This function clears the current engine worker and resets the compute state to Idle.
	 * It is typically called when the engine is no longer needed.
	 */
    void stopEngine() {
		if (!engine_) {
            return;
        }
		engine_->stop();
        computeState_ = ComputeState::Idle;
	}

    /**
     * @brief Restarts the engine process.
     * 
	 * @param outsideThread If true, the restart will be performed outside the engine thread.
     *
     * Terminates the current engine instance and starts a new one.
     */
    void restartEngine(bool outsideThread = false);

    /**
     * @brief Returns a raw pointer to the EngineWorker instance.
     *
     * @return Pointer to the EngineWorker.
     */
    EngineWorker* getEngine() {
        return engine_.get();
    }
    const EngineWorker* getEngine() const {
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
    void newGame(const GameRecord& gameRecord, bool engineIsWhite) {
        timeControl_ = engineIsWhite ? gameRecord.getWhiteTimeControl() : gameRecord.getBlackTimeControl();
        if (engine_) {
            engine_->newGame(gameRecord, engineIsWhite);
        }
    }

    /**
	 * @brief Tells the engine to compute a new move
	 * @param gameRecord The current game with startposition and moves played so far.
	 * @param goLimits The time limits for the next move
	 * @param analyze If true, the engine will analyze the position without playing the move.
     */
    void computeMove(const GameRecord& gameRecord, const GoLimits& goLimits, bool analyze = false);


    /**
	 * @brief Allows the engine to ponder during its turn.
	 * @param gameRecord The current game with startposition and moves played so far.
	 * @param goLimits The time limits for the next move
	 * @param event the event that triggered the pondering, if any
     */
    void allowPonder(const GameRecord& gameRecord, const GoLimits& goLimits, 
        const std::optional<EngineEvent>& event = std::nullopt);

    /**
     * @brief Checks if the engine is currently pondering.
     * 
     * @return True if the engine is pondering, false otherwise.
     */
    bool isPondering() const {
        return computeState_ == ComputeState::PonderingOrIdle;
    }

	/**
	 * @brief Cancels the current move computation.
	 *
	 * This function requests the engine to cancel computing the current move.
	 * If the engine responds with a move, it will be ignored.
	 */
    void cancelCompute();

    /**
     * @brief Sends an isready command to the engine and waits for readyok.
     * 
     * @param timeout Maximum time to wait for readyok response.
     */
    void checkReady(std::chrono::milliseconds timeout = std::chrono::seconds{1}) {
        if (!engine_) {
            return;
        }
        engine_->requestReady(timeout);
    }

    /**
	 * @brief Tells the engine to stop computing and return the best move.
     */
    void moveNow() {
        if (!engine_) {
            return;
        }
        if (computeState_ == ComputeState::ComputingMove) {
            engine_->moveNow();
        }
	}

	/**
	 * @brief returns player to move
	 */
	bool isWhiteToMove() const {
		return gameState_.isWhiteToMove();
	}

	/**
	 * @brief Returns the current move record.
	 * It is not thread-safe to call this function as it returns a reference to the current move.
	 */
	const MoveRecord& getCurrentMove() const {
		return currentMove_;
	}

    /**
	 * @brief Returns a thread save copy of the current move record.
     */
    MoveRecord getCurrentMoveCopy() const {
        std::scoped_lock lock(currentMoveMutex_);
        return currentMove_;
	}

    /**
     * @brief Executes the given callable with thread-safe access to the current move.
     * @param accessFn A callable that takes a const MoveRecord&.
     */
    void withCurrentMove(const std::function<void(const MoveRecord&)>& accessFn) const {
        std::scoped_lock lock(currentMoveMutex_);
        accessFn(currentMove_);
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
    void setComputeMoveStartTimestamp(uint64_t timestamp) {
        computeMoveStartTimestamp_ = std::max(computeMoveStartTimestamp_, timestamp);
		// I expect this to never happen. Let us see in debug mode. If this happens this should be a race condition.
        // But it still make sense to check, if this is a bug in the code
        // timestamp = 0 indicates that the command could not be fulfilled by the adapter what is a valid case
        assert(timestamp == 0 || timestamp >= computeMoveStartTimestamp_);
    }

    /**
     * @brief Sets the flag indicating whether a compute move is currently active.
     *
     * This flag is updated asynchronously in response to a marker event (SendingComputeMove),
     * ensuring that info packets from previous pondering phases are not misinterpreted as part
     * of the current move computation.
     *
     */
    void setComputingMove() {
        computeState_ = ComputeState::ComputingMove;
    }

    /**
     * @brief Gets the timestamp when the engine started computing a move.
     *
     * @return Timestamp in milliseconds since epoch.
     */
    uint64_t getComputeMoveStartTimestamp() const {
        return computeMoveStartTimestamp_;
    }

    /**
     * @brief Handles an info event from the engine.
     *
     * @param event The EngineEvent containing the information.
     */
    void handleInfo(const EngineEvent& event);

    /**
     * @brief Checks the PV (principal variation) in the event.
     *
     * This function verifies that the PV in the event matches the current game state.
     * If it does not match, it logs an error and updates the move record accordingly.
     *
     * @param event The EngineEvent containing the PV to check.
     */
    void checkPV(const EngineEvent& event);

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
     * @brief Handles a ponder move hint from the engine.
     *
     * XBoard engines may send "Hint: <move>" before thinking lines when pondering in post mode.
     * The hint indicates which opponent move the engine assumes and is pondering on.
     * This stores the hint internally for tracking the ponder state.
     *
     * @param event The EngineEvent containing the ponder move hint.
     */
    void handlePonderMove(const EngineEvent& event);

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
     * @brief Plays a move in the game based on an external move information.
     *
     * This function updates the game state with the given move and updates the current move record.
     * It is used to apply a move to the game state, typically after validating the move.
     *
     * @param move The MoveRecord containing the move to play.
	 */
    void doMove(const MoveRecord& move);
    
    /**
	 * @brief Sets the game state from a GameRecord.
	 * @param startPosition The GameRecord to set the game state from.
     */
    void setStartPosition(const GameRecord& startPosition) {
        std::scoped_lock lock(stateMutex_);
        gameState_.setFromGameRecord(startPosition, startPosition.nextMoveIndex());
        ponderState_.setFromGameRecord(startPosition, startPosition.nextMoveIndex());
    }

    /**
     * @brief Sets the game state to a new position.
     *
     * @param startPosition If true, sets the game to the starting position.
     * @param fen The FEN string representing the new position.
     */
    void setStartPosition(bool startPosition, const std::string& fen) {
        std::scoped_lock lock(stateMutex_);
        gameState_.setFen(startPosition, fen);
        ponderState_.setFen(startPosition, fen);
    }

	const TimeControl& getTimeControl() const {
		return timeControl_;
	}

private:
    enum class ComputeState: std::uint8_t {
        Idle,
        ComputingMove,
        StoppingMove,
        PonderingOrIdle,
        PonderHit,
        PonderMiss
    };
    static const char* toString(ComputeState state) {
        switch (state) {
            case ComputeState::ComputingMove:return "ComputingMove";
            case ComputeState::PonderingOrIdle: return "Pondering or idle";
            case ComputeState::PonderHit:    return "PonderHit";
            case ComputeState::PonderMiss:   return "PonderMiss";
			default: return "Idle";
        }
    }

    bool restartIfNotReady();

    /**
     * @brief Validates a PV against a game state.
     * @return Empty string if all moves valid, otherwise the invalid move string.
     */
    std::string validatePVAgainstState(GameState& state, const std::vector<std::string>& pv) const;

    /**
     * @brief Sets up ponder state with the given move if valid.
     * @param move The ponder move string to validate and apply.
     * @param rawLine The raw engine output line for error reporting.
     * @return true if ponder state was set up successfully, false otherwise.
     */
    bool setupPonderState(const std::string& move, const std::string& rawLine);

    /**
     * @brief Checks if the GUI assumes the engine is pondering on a specific move.
     * 
     * Returns true only when both conditions are met:
     * - computeState_ == PonderingOrIdle (engine was allowed to ponder)
     * - ponderMove_ is not empty (we know which move to ponder on)
     * 
     * This is an assumption because:
     * - Engines may choose not to ponder or have no pondering implemented even.
     * - We treat the state as "pondering" for PV validation and ponder hit/miss logic
     */
    bool isAssumedPondering() const {
        return computeState_ == ComputeState::PonderingOrIdle && !ponderMove_.empty();
    }


    std::unique_ptr<EngineWorker> engine_;
    TimeControl timeControl_;
    GameState gameState_;
    GameState ponderState_;
    uint64_t computeMoveStartTimestamp_ = 0;
    GoLimits goLimits_;
    bool requireLan_;
    std::atomic<ComputeState> computeState_ = ComputeState::Idle;
	bool isAnalyzing_ = false;

    std::string ponderMove_;
    MoveRecord currentMove_;
    mutable std::mutex currentMoveMutex_;
    mutable std::mutex stateMutex_; // protects gameState_ and ponderState_
	EngineReport* checklist_ = nullptr; 
};

} // namespace QaplaTester
