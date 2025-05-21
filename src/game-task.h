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

#include <optional>
#include <string>
#include "game-record.h"

struct GameTask {
    enum class Type {
        None,
        ComputeMove,
        PlayGame
    };
    bool useStartPosition;
    Type taskType;
    std::string fen;
    TimeControl whiteTimeControl;
    TimeControl blackTimeControl;
};

class GameTaskProvider {
public:
	GameTaskProvider() = default;
	virtual ~GameTaskProvider() = default;
	/**
	 * @brief Provides the next game task.
	 * @return An optional GameTask. If no more tasks are available, returns std::nullopt.
	 */
	virtual std::optional<GameTask> nextTask() = 0;
    /**
	 * @brief Sets the game state for the task.
	 * @param state The game state to set.
     */
    virtual void setGameRecord(const GameRecord& record) = 0;
};
