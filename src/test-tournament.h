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
#include <utility>
#include "time-control.h"
#include "game-record.h"
#include "game-result.h"
#include "engine-report.h"
#include "game-task.h"

class TestTournament : public GameTaskProvider {
public:
    explicit TestTournament(uint32_t totalGames, EngineReport* checklist)
        : maxGames_(totalGames), current_(0), checklist_(checklist) {
        timePairs_ = {
            {{0, 20000, 500}, {0, 10000, 100}},
            {{0, 10000, 500}, {0,  5000, 100}},
            {{0,  4000, 500}, {0,  2000, 100}},
            {{0, 20000, 500}, {0, 10000,   0}},
            {{0, 10000, 200}, {0,  5000,   0}},
            {{0,  6000, 200}, {0,  3000,   0}}
        };
    }

    /**
     * @brief Provides the next available task.
     *
     * @return A GameTask with a unique taskId or std::nullopt if no task is available.
     */
    std::optional<GameTask> nextTask() override 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_ >= maxGames_) return std::nullopt;

		size_t numPairs = timePairs_.size();
		size_t divisor = (maxGames_ + numPairs - 1) / numPairs;
        size_t idx = current_ / divisor;
        ++current_;

        GameTask task;
        task.gameRecord.setStartPosition(true, "", true, "", "");
		task.gameRecord.getWhiteTimeControl().addTimeSegment(timePairs_[idx].first);
		task.gameRecord.getBlackTimeControl().addTimeSegment(timePairs_[idx].second);
		task.taskType = GameTask::Type::PlayGame;
        return task;
    }

    /**
     * @brief Records the result of a finished game identified by taskId.
     *
     * @param taskId Identifier of the task this game result belongs to.
     * @param record Game outcome and metadata.
     */
    void setGameRecord(
        [[maybe_unused]] const std::string & taskId,
        const GameRecord & record) override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            gameRecords_.push_back(record);
        }
		checkTimeManagement(record);
        logStatus();
    }

    void checkTimeManagement(const GameRecord& record) {
        const auto [cause, result] = record.getGameResult();
        bool success = (cause != GameEndCause::Timeout);
		std::string whiteTimeControl = record.getWhiteTimeControl().toPgnTimeControlString();
		std::string blackTimeControl = record.getBlackTimeControl().toPgnTimeControlString();

        checklist_->logReport("no-loss-on-time", success, " looses on time with time control " +
            (result == GameResult::WhiteWins ? blackTimeControl : whiteTimeControl) );

        timeUsageReasonable(record.timeUsed().first,
            record.getWhiteTimeControl(),
            record.history().size());
        timeUsageReasonable(record.timeUsed().second,
            record.getBlackTimeControl(),
            record.history().size());
        
    }

    /**
     * Calculates the expected range of used time ratio at the end of a game based on move count.
     * The ratio refers to (usedTime / availableTime), and acceptable bounds are interpolated
     * from predefined ranges to reflect sensible engine time consumption.
     *
     * @param moveCount The total number of moves in the game.
     * @return A pair of (minUsageRatio, maxUsageRatio), both in the range [0.0, 1.0].
     */
    std::pair<double, double> expectedUsageRatioRange(size_t moveCount) {
        struct UsageProfile {
            size_t moveThreshold;
            double minRatio;
            double maxRatio;
        };

        constexpr UsageProfile usageTable[] = {
            {0,   0.00, 0.20},
            {40,  0.20, 0.60},
            {80,  0.40, 0.90},
            {160, 0.65, 1.00},
            {320, 0.80, 1.00},
        };

        for (size_t i = 1; i < std::size(usageTable); ++i) {
            if (moveCount < usageTable[i].moveThreshold) {
                const auto& low = usageTable[i - 1];
                const auto& high = usageTable[i];
                double factor = static_cast<double>(moveCount - low.moveThreshold) /
                    static_cast<double>(high.moveThreshold - low.moveThreshold);
                double minRatio = low.minRatio + factor * (high.minRatio - low.minRatio);
                double maxRatio = low.maxRatio + factor * (high.maxRatio - low.maxRatio);
                return { minRatio, maxRatio };
            }
        }

        const auto& last = usageTable[std::size(usageTable) - 1];
        return { last.minRatio, last.maxRatio };
    }

    void timeUsageReasonable(uint64_t usedTimeMs, const TimeControl& tc, size_t moveCount) {
        if (moveCount < 30) return;

        auto segments = tc.timeSegments();
        if (segments.empty()) return;

        const auto& seg = segments.front();
        uint64_t availableTime = seg.baseTimeMs + moveCount * seg.incrementMs;
        if (availableTime == 0) return;

        double usageRatio = static_cast<double>(usedTimeMs) / static_cast<double>(availableTime);
        auto [minRatio, maxRatio] = expectedUsageRatioRange(moveCount);
		double incMs = static_cast<double>(seg.incrementMs);
		double baseMs = static_cast<double>(seg.baseTimeMs);
        minRatio += (1.0 - minRatio) * std::min(1.0, incMs * 20.0 / (baseMs + 1));
        maxRatio += (1.0 - maxRatio) * std::min(1.0, incMs * 100.0 / (baseMs + 1));

        uint64_t timeLeft = availableTime - usedTimeMs;

        bool inMaxRange = usageRatio <= maxRatio;
        std::string detail = "time control " + tc.toPgnTimeControlString()
            + " used " + std::to_string(usedTimeMs) + "ms, ratio: "
            + std::to_string(usageRatio) + ", expected [" + std::to_string(minRatio)
            + ", " + std::to_string(maxRatio) + "], move count " + std::to_string(moveCount)
            + " time left: " + std::to_string(timeLeft) + "ms";

        if (checkTimeLimits) {
            checklist_->logReport("keeps-reserve-time", inMaxRange, detail);
            checklist_->logReport("not-below-one-second", timeLeft >= 1000,
                " time control: " + tc.toPgnTimeControlString() + " time left: " + std::to_string(timeLeft) + "ms");
        }
    }


    void logStatus() {
        std::lock_guard<std::mutex> lock(mutex_);
		auto& lastWhiteTimeControl = gameRecords_.back().getWhiteTimeControl();
		auto& lastBlackTimeControl = gameRecords_.back().getBlackTimeControl();
		std::string whiteTimeControl = lastWhiteTimeControl.toPgnTimeControlString();
		std::string blackTimeControl = lastBlackTimeControl.toPgnTimeControlString();

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
		oss << whiteTimeControl << " vs. " << blackTimeControl << " | ";
        for (const auto& [cause, count] : causeCounts)
            oss << to_string(cause) << ":" << count << " ";

        Logger::testLogger().log(oss.str());
    }

    bool checkTimeLimits = false;
private:
    uint32_t maxGames_;
    uint32_t current_;
    std::mutex mutex_;
    std::vector<GameRecord> gameRecords_;
    std::vector<std::pair<TimeSegment, TimeSegment>> timePairs_;
    std::vector<int> usageCount_;
    EngineReport* checklist_;
};
