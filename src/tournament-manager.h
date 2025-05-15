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
#include <map>
#include <mutex>
#include "time-control.h"
#include "game-record.h"
#include "game-result.h"

struct GameTask {
    bool useStartPosition;
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

class TournamentManager : public GameTaskProvider {
public:
    explicit TournamentManager(int totalGames)
        : maxGames_(totalGames), current_(0) {
        int usesPerPair = maxGames_ / 20;
        usageCount_.resize(20, 0);
        timePairs_ = {
            {{0, 200000, 500}, {0, 100000, 100}},
            {{0, 180000, 500}, {0,  90000, 100}},
            {{0, 160000, 500}, {0,  80000, 100}},
            {{0, 140000, 500}, {0,  70000, 100}},
            {{0, 120000, 500}, {0,  60000, 100}},
            {{0, 100000, 500}, {0,  50000, 100}},
            {{0,  80000, 500}, {0,  40000, 100}},
            {{0,  60000, 500}, {0,  30000, 100}},
            {{0,  40000, 500}, {0,  20000, 100}},
            {{0,  20000, 500}, {0,  10000, 100}},
            {{0, 200000, 500}, {0, 100000,   0}},
            {{0, 180000, 200}, {0,  90000,   0}},
            {{0, 160000, 200}, {0,  80000,   0}},
            {{0, 140000, 200}, {0,  70000,   0}},
            {{0, 120000, 200}, {0,  60000,   0}},
            {{0, 100000, 200}, {0,  50000,   0}},
            {{0,  80000, 200}, {0,  40000,   0}},
            {{0,  60000, 200}, {0,  30000,   0}},
            {{0,  40000, 200}, {0,  20000,   0}},
            {{0,  20000, 200}, {0,  10000,   0}}
        };
    }

    std::optional<GameTask> nextTask() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_ >= maxGames_) return std::nullopt;

        for (size_t i = 0; i < 20; ++i) {
            if (usageCount_[i] < maxGames_ / 20) {
                GameTask task;
                task.useStartPosition = true;
                task.fen = "";
                task.whiteTimeControl.addTimeSegment(timePairs_[i].first);
                task.blackTimeControl.addTimeSegment(timePairs_[i].second);
                ++usageCount_[i];
                ++current_;
                return task;
            }
        }

        return std::nullopt;
    }

    void setGameRecord(const GameRecord& record) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            gameRecords_.push_back(record);
        }
        logStatus();
    }

    void logStatus() {
        std::lock_guard<std::mutex> lock(mutex_);

        int whiteWins = 0, blackWins = 0, draws = 0;
        std::map<GameEndCause, int> causeCounts;

        for (const auto& game : gameRecords_) {
            auto [cause, result] = game.getGameResult();
            switch (result) {
            case GameResult::WhiteWins: ++whiteWins; break;
            case GameResult::BlackWins: ++blackWins; break;
            case GameResult::Draw: ++draws; break;
            default: break;
            }
            if (cause != GameEndCause::Ongoing)
                ++causeCounts[cause];
        }

        std::ostringstream oss;
        oss << "[" << std::setw(3) << gameRecords_.size() << "/" << maxGames_ << "] "
            << "W:" << std::setw(3) << whiteWins
            << " D:" << std::setw(3) << draws
            << " B:" << std::setw(3) << blackWins
            << " | ";

        for (const auto& [cause, count] : causeCounts)
            oss << to_string(cause) << ":" << count << " ";

        Logger::testLogger().log(oss.str());
    }

private:
    int maxGames_;
    int current_;
    std::mutex mutex_;
    std::vector<GameRecord> gameRecords_;
    std::vector<std::pair<TimeSegment, TimeSegment>> timePairs_;
    std::vector<int> usageCount_;
};
