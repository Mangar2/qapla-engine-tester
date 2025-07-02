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

#include "tournament-result.h"
#include "elo-helper.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

void EngineDuelResult::addResult(const GameRecord& record) {
    bool engineAIsWhite = engineA == record.getWhiteEngineName();
    auto [cause, result] = record.getGameResult();

    if ((result == GameResult::WhiteWins && engineAIsWhite) ||
        (result == GameResult::BlackWins && !engineAIsWhite)) {
        ++winsEngineA;
        ++causeStats[static_cast<size_t>(cause)].win;
    }
    else if ((result == GameResult::WhiteWins && !engineAIsWhite) ||
        (result == GameResult::BlackWins && engineAIsWhite)) {
        ++winsEngineB;
        ++causeStats[static_cast<size_t>(cause)].loss;
    }
    else if (result == GameResult::Draw) {
        ++draws;
        ++causeStats[static_cast<size_t>(cause)].draw;
    }
}

EngineDuelResult EngineDuelResult::switchedSides() const {
    EngineDuelResult result(engineB, engineA);
    result.winsEngineA = winsEngineB;
    result.winsEngineB = winsEngineA;
    result.draws = draws;

    for (size_t i = 0; i < static_cast<size_t>(GameEndCause::Count); ++i) {
        result.causeStats[i].win = causeStats[i].loss;
        result.causeStats[i].loss = causeStats[i].win;
        result.causeStats[i].draw = causeStats[i].draw;
    }
    return result;
}

EngineDuelResult& EngineDuelResult::operator+=(const EngineDuelResult& other) {
    const bool sameDirect = engineA == other.engineA && (engineB == other.engineB || engineB == ANY_ENGINE);
    const bool sameReverse = engineA == other.engineB && (engineB == other.engineA || engineB == ANY_ENGINE);

    if (!sameDirect && !sameReverse) {
        throw std::invalid_argument("Cannot add EngineDuelResult: mismatched engine pairs.");
    }

    if (sameDirect) {
        winsEngineA += other.winsEngineA;
        winsEngineB += other.winsEngineB;
        for (size_t i = 0; i < causeStats.size(); ++i) {
            causeStats[i].win += other.causeStats[i].win;
            causeStats[i].loss += other.causeStats[i].loss;
            causeStats[i].draw += other.causeStats[i].draw;
        }
    }
    else {
        winsEngineA += other.winsEngineB;
        winsEngineB += other.winsEngineA;
        for (size_t i = 0; i < causeStats.size(); ++i) {
            causeStats[i].win += other.causeStats[i].loss;
            causeStats[i].loss += other.causeStats[i].win;
            causeStats[i].draw += other.causeStats[i].draw;
        }
    }

    draws += other.draws;
    return *this;
}

EngineDuelResult EngineResult::aggregate(const std::string& targetEngine) const {
    if (duels.empty()) return {};

    EngineDuelResult result(targetEngine, std::string(EngineDuelResult::ANY_ENGINE));

    for (const auto& duel : duels) {
        EngineDuelResult aligned = (duel.getEngineA() == targetEngine) ? duel : duel.switchedSides();
        result += aligned;
    }

    return result;
}

void EngineResult::writeTo(std::ostream& os) const {
    EngineDuelResult total = aggregate(engineName);

    os << std::left << std::setw(30) << "Overall:"
        << " " << total.toResultString() << "\n";

    for (const auto& duel : duels) {
        os << std::left << std::setw(30) << duel.getEngineB()
            << " " << duel.toResultString() << "\n";
    }

    os << "\nWin Causes:\n";
    for (size_t i = 0; i < total.causeStats.size(); ++i) {
        if (total.causeStats[i].win > 0) {
            os << " - " << to_string(static_cast<GameEndCause>(i))
                << ": " << total.causeStats[i].win << "\n";
        }
    }

    os << "\nDraw Causes:\n";
    for (size_t i = 0; i < total.causeStats.size(); ++i) {
        if (total.causeStats[i].draw > 0) {
            os << " - " << to_string(static_cast<GameEndCause>(i))
                << ": " << total.causeStats[i].draw << "\n";
        }
    }

    os << "\nLoss Causes:\n";
    for (size_t i = 0; i < total.causeStats.size(); ++i) {
        if (total.causeStats[i].loss > 0) {
            os << " - " << to_string(static_cast<GameEndCause>(i))
                << ": " << total.causeStats[i].loss << "\n";
        }
    }
}


void TournamentResult::add(const EngineDuelResult& result) {
    results_.push_back(result);
}

std::vector<std::string> TournamentResult::engineNames() const {
    std::unordered_set<std::string> names;
    for (const auto& duel : results_) {
        names.insert(duel.getEngineA());
        names.insert(duel.getEngineB());
    }
    return std::vector<std::string>(names.begin(), names.end());
}

std::optional<EngineResult> TournamentResult::forEngine(const std::string& name) const {
    std::unordered_map<std::string, EngineDuelResult> aggregated;

    for (const auto& duel : results_) {
        if (duel.getEngineA() == name || duel.getEngineB() == name) {
            EngineDuelResult aligned = (duel.getEngineA() == name) ? duel : duel.switchedSides();
            auto it = aggregated.find(aligned.getEngineB());
            if (it == aggregated.end()) {
                aggregated[aligned.getEngineB()] = aligned;
            }
            else {
                it->second += aligned;
            }
        }
    }

    if (aggregated.empty()) return std::nullopt;

    EngineResult result;
	result.engineName = name;
    for (auto& [_, duel] : aggregated) {
        result.duels.push_back(std::move(duel));
    }

    return result;
}

void TournamentResult::printRatingTableUciStyle(std::ostream& os) const {
    struct Scored {
        EngineDuelResult result;
        double score;
        int elo;
        int error;
    };

    std::vector<Scored> list;

    for (const auto& name : engineNames()) {
        auto opt = forEngine(name);
        if (!opt) continue;

        EngineDuelResult agg = opt->aggregate(name);
        const int total = agg.total();
        if (total == 0) continue;

        double score = (agg.winsEngineA + 0.5 * agg.draws) / total;
        auto [elo, err] = computeEloWithError(agg.winsEngineA, agg.winsEngineB, agg.draws);
        list.push_back({ std::move(agg), score, elo, err });
    }

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });

    int rank = 1;
    for (const auto& entry : list) {
        const auto& r = entry.result;
        const int total = r.total();
        double drawPct = 100.0 * r.draws / total;
        double scorePct = 100.0 * entry.score;

        os << "rank " << rank++
           << " name " << r.getEngineA()
           << " elo " << entry.elo
           << " +/- " << entry.error
           << " games " << total
           << " score " << std::fixed << std::setprecision(1) << scorePct << "%"
           << " draw " << drawPct << "%\n";
    }
}

void TournamentResult::printSummary(std::ostream& os) const {
    os << "\nTournament result:\n";

    std::vector<std::pair<double, std::string>> lines;

    for (const auto& name : engineNames()) {
        auto optResult = forEngine(name);
        if (!optResult) continue;

        EngineDuelResult agg = optResult->aggregate(name);
        int wins = agg.winsEngineA;
        int losses = agg.winsEngineB;
        int draws = agg.draws;
        int total = wins + losses + draws;

        double score = total > 0 ? (wins + 0.5 * draws) / total : 0.0;

        std::ostringstream line;
        line << std::fixed << std::setprecision(2)
            << std::setw(30) << std::left << name
            << " " << score
            << "  W:" << wins << " D:" << draws << " L:" << losses;

        lines.emplace_back(score, line.str());
    }

    std::sort(lines.begin(), lines.end(), [](const auto& a, const auto& b) {
        return b.first < a.first; // descending
        });

    for (const auto& entry : lines) {
        os << entry.second << "\n";
    }
    os << std::endl;
}


