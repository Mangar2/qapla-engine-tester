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
#include "time-control.h"

struct GameTask {
    bool useStartPosition;
    std::string fen;
    TimeControl whiteTimeControl;
    TimeControl blackTimeControl;
};

class TournamentManager {
public:
    explicit TournamentManager(int totalGames)
        : maxGames_(totalGames), current_(0) {
    }

    std::optional<GameTask> nextTask() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_ >= maxGames_) return std::nullopt;

        GameTask task;
        task.useStartPosition = true;
        task.fen = "";
        task.whiteTimeControl.addTimeSegment({ 0, 30000, 500 });
        task.blackTimeControl.addTimeSegment({ 0, 10000, 100 });

        ++current_;
        return task;
    }

private:
    int maxGames_;
    int current_;
    std::mutex mutex_;
};
