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

#include "test-tournament.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <array>

namespace QaplaTester {

TestTournament::TestTournament(uint32_t totalGames, EngineReport* checklist)
    : maxGames_(totalGames), checklist_(checklist) {
    timePairs_ = {
        {{.movesToPlay=0, .baseTimeMs=20000, .incrementMs=500}, {.movesToPlay=0, .baseTimeMs=10000, .incrementMs=100}},
        {{.movesToPlay=0, .baseTimeMs=10000, .incrementMs=500}, {.movesToPlay=0, .baseTimeMs=5000, .incrementMs=100}},
        {{.movesToPlay=0, .baseTimeMs=4000, .incrementMs=500}, {.movesToPlay=0, .baseTimeMs=2000, .incrementMs=100}},
        {{.movesToPlay=0, .baseTimeMs=20000, .incrementMs=500}, {.movesToPlay=0, .baseTimeMs=10000, .incrementMs=0}},
        {{.movesToPlay=0, .baseTimeMs=10000, .incrementMs=200}, {.movesToPlay=0, .baseTimeMs=5000, .incrementMs=0}},
        {{.movesToPlay=0, .baseTimeMs=6000, .incrementMs=200}, {.movesToPlay=0, .baseTimeMs=3000, .incrementMs=0}}
    };
}

std::optional<GameTask> TestTournament::nextTask() {
    std::scoped_lock lock(mutex_);
    if (current_ >= maxGames_) {
        return std::nullopt;
    }

    size_t numPairs = timePairs_.size();
    size_t divisor = (maxGames_ + numPairs - 1) / numPairs;
    size_t idx = current_ / divisor;
    ++current_;

    GameTask task;
    task.gameRecord.setStartPosition(true, "", true, 0, "", "");
    task.gameRecord.getWhiteTimeControl().addTimeSegment(timePairs_[idx].first);
    task.gameRecord.getBlackTimeControl().addTimeSegment(timePairs_[idx].second);
    task.gameRecord.setTournamentInfo(1, current_, 0);
    task.taskType = GameTask::Type::PlayGame;
    return task;
}

void TestTournament::setGameRecord(
    [[maybe_unused]] const std::string& taskId,
    const GameRecord& record) {
    {
        std::scoped_lock lock(mutex_);
        gameRecords_.push_back(record);
    }
    checkTimeManagement(record);
    logStatus();
}

void TestTournament::checkTimeManagement(const GameRecord& record) {
    const auto [cause, result] = record.getGameResult();
    bool success = (cause != GameEndCause::Timeout);
    std::string whiteTimeControl = record.getWhiteTimeControl().toPgnTimeControlString();
    std::string blackTimeControl = record.getBlackTimeControl().toPgnTimeControlString();

    checklist_->logReport("no-loss-on-time", success, " looses on time with time control " +
        (result == GameResult::WhiteWins ? blackTimeControl : whiteTimeControl));

    timeUsageReasonable(record.timeUsed().first,
        record.getWhiteTimeControl(),
        record.history().size());
    timeUsageReasonable(record.timeUsed().second,
        record.getBlackTimeControl(),
        record.history().size());
}

std::pair<double, double> TestTournament::expectedUsageRatioRange(size_t moveCount) {
    struct UsageProfile {
        size_t moveThreshold;
        double minRatio;
        double maxRatio;
    };

    constexpr std::array<UsageProfile, 5> usageTable = {{
        {.moveThreshold=0, .minRatio=0.00, .maxRatio=0.20},
        {.moveThreshold=40, .minRatio=0.20, .maxRatio=0.60},
        {.moveThreshold=80, .minRatio=0.40, .maxRatio=0.90},
        {.moveThreshold=160, .minRatio=0.65, .maxRatio=1.00},
        {.moveThreshold=320, .minRatio=0.80, .maxRatio=1.00},
    }};

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

void TestTournament::timeUsageReasonable(uint64_t usedTimeMs, const TimeControl& tc, size_t moveCount) {
    if (moveCount < 30) {
        return;
    }

    auto segments = tc.timeSegments();
    if (segments.empty()) {
        return;
    }

    const auto& seg = segments.front();
    uint64_t availableTime = seg.baseTimeMs + moveCount * seg.incrementMs;
    if (availableTime == 0) {
        return;
    }

    double usageRatio = static_cast<double>(usedTimeMs) / static_cast<double>(availableTime);
    auto [minRatio, maxRatio] = expectedUsageRatioRange(moveCount);
    auto incMs = static_cast<double>(seg.incrementMs);
    auto baseMs = static_cast<double>(seg.baseTimeMs);
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

void TestTournament::logStatus() {
    std::scoped_lock lock(mutex_);
    auto& lastWhiteTimeControl = gameRecords_.back().getWhiteTimeControl();
    auto& lastBlackTimeControl = gameRecords_.back().getBlackTimeControl();
    std::string whiteTimeControl = lastWhiteTimeControl.toPgnTimeControlString();
    std::string blackTimeControl = lastBlackTimeControl.toPgnTimeControlString();

    int whiteWins = 0;
    int blackWins = 0;
    int draws = 0;
    std::map<GameEndCause, int> causeCounts;

    for (const auto& game : gameRecords_) {
        auto [cause, result] = game.getGameResult();
        switch (result) {
        case GameResult::WhiteWins: ++whiteWins; break;
        case GameResult::BlackWins: ++blackWins; break;
        case GameResult::Draw: ++draws; break;
        default: break;
        }
        if (cause != GameEndCause::Ongoing) {
            ++causeCounts[cause];
        }
    }

    std::ostringstream oss;
    oss << "[" << std::setw(3) << gameRecords_.size() << "/" << maxGames_ << "] "
        << "W:" << std::setw(3) << whiteWins
        << " D:" << std::setw(3) << draws
        << " B:" << std::setw(3) << blackWins
        << " | ";
    oss << whiteTimeControl << " vs. " << blackTimeControl << " | ";
    for (const auto& [cause, count] : causeCounts) {
        oss << to_string(cause) << ":" << count << " ";
    }

    Logger::testLogger().log(oss.str());
}

} // namespace QaplaTester
