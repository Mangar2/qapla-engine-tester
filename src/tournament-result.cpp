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
#include <iomanip>
#include <iostream>
#include <sstream>

namespace QaplaTester {

void EngineDuelResult::addResult(const GameRecord &record)
{
    bool engineAIsWhite = engineA == record.getWhiteEngineName();
    auto [cause, result] = record.getGameResult();

    if ((result == GameResult::WhiteWins && engineAIsWhite) ||
        (result == GameResult::BlackWins && !engineAIsWhite))
    {
        ++winsEngineA;
        ++causeStats[static_cast<size_t>(cause)].win;
    }
    else if ((result == GameResult::WhiteWins && !engineAIsWhite) ||
             (result == GameResult::BlackWins && engineAIsWhite))
    {
        ++winsEngineB;
        ++causeStats[static_cast<size_t>(cause)].loss;
    }
    else if (result == GameResult::Draw)
    {
        ++draws;
        ++causeStats[static_cast<size_t>(cause)].draw;
    }
}

EngineDuelResult EngineDuelResult::switchedSides() const
{
    EngineDuelResult result(engineB, engineA);
    result.winsEngineA = winsEngineB;
    result.winsEngineB = winsEngineA;
    result.draws = draws;

    for (size_t i = 0; i < static_cast<size_t>(GameEndCause::Count); ++i)
    {
        result.causeStats[i].win = causeStats[i].loss;
        result.causeStats[i].loss = causeStats[i].win;
        result.causeStats[i].draw = causeStats[i].draw;
    }
    return result;
}

EngineDuelResult &EngineDuelResult::operator+=(const EngineDuelResult &other)
{
    const bool sameDirect = engineA == other.engineA && (engineB == other.engineB || engineB == ANY_ENGINE);
    const bool sameReverse = engineA == other.engineB && (engineB == other.engineA || engineB == ANY_ENGINE);

    if (!sameDirect && !sameReverse)
    {
        throw std::invalid_argument("Cannot add EngineDuelResult: mismatched engine pairs.");
    }

    if (sameDirect)
    {
        winsEngineA += other.winsEngineA;
        winsEngineB += other.winsEngineB;
        for (size_t i = 0; i < causeStats.size(); ++i)
        {
            causeStats[i].win += other.causeStats[i].win;
            causeStats[i].loss += other.causeStats[i].loss;
            causeStats[i].draw += other.causeStats[i].draw;
        }
    }
    else
    {
        winsEngineA += other.winsEngineB;
        winsEngineB += other.winsEngineA;
        for (size_t i = 0; i < causeStats.size(); ++i)
        {
            causeStats[i].win += other.causeStats[i].loss;
            causeStats[i].loss += other.causeStats[i].win;
            causeStats[i].draw += other.causeStats[i].draw;
        }
    }

    draws += other.draws;
    return *this;
}

EngineDuelResult EngineResult::aggregate(const std::string &targetEngine) const
{
    if (duels.empty()) {
        return {};
    }

    EngineDuelResult result(targetEngine, std::string(EngineDuelResult::ANY_ENGINE));

    for (const auto &duel : duels)
    {
        EngineDuelResult aligned = (duel.getEngineA() == targetEngine) ? duel : duel.switchedSides();
        result += aligned;
    }

    return result;
}

void EngineResult::printOutcome(std::ostream &os) const
{
    EngineDuelResult total = aggregate(engineName);

    for (size_t i = 0; i < total.causeStats.size(); ++i)
    {
        if (total.causeStats[i].win > 0)
        {
            os << "engine " << std::setw(25) << engineName
               << "win  " << std::setw(22) << to_string(static_cast<GameEndCause>(i))
               << " " << total.causeStats[i].win << "\n";
        }
    }
    for (size_t i = 0; i < total.causeStats.size(); ++i)
    {
        if (total.causeStats[i].draw > 0)
        {
            os << "engine " << std::setw(25) << engineName
               << "draw " << std::setw(22) << to_string(static_cast<GameEndCause>(i))
               << " " << total.causeStats[i].draw << "\n";
        }
    }
    for (size_t i = 0; i < total.causeStats.size(); ++i)
    {
        if (total.causeStats[i].loss > 0)
        {
            os << "engine " << std::setw(25) << engineName
               << "loss " << std::setw(22) << to_string(static_cast<GameEndCause>(i))
               << " " << total.causeStats[i].loss << "\n";
        }   
    } 
}

void EngineResult::printResults(std::ostream &os) const
{
    EngineDuelResult total = aggregate(engineName);

    os << std::left << std::setw(30) << "Overall:"
       << " " << total.toResultString() << "\n";

    for (const auto &duel : duels)
    {
        os << std::left << std::setw(30) << duel.getEngineB()
           << " " << duel.toResultString() << "\n";
    }

    os << "\nWin Causes:\n";
    for (size_t i = 0; i < total.causeStats.size(); ++i)
    {
        if (total.causeStats[i].win > 0)
        {
            os << " - " << to_string(static_cast<GameEndCause>(i))
               << ": " << total.causeStats[i].win << "\n";
        }
    }

    os << "\nDraw Causes:\n";
    for (size_t i = 0; i < total.causeStats.size(); ++i)
    {
        if (total.causeStats[i].draw > 0)
        {
            os << " - " << to_string(static_cast<GameEndCause>(i))
               << ": " << total.causeStats[i].draw << "\n";
        }
    }

    os << "\nLoss Causes:\n";
    for (size_t i = 0; i < total.causeStats.size(); ++i)
    {
        if (total.causeStats[i].loss > 0)
        {
            os << " - " << to_string(static_cast<GameEndCause>(i))
               << ": " << total.causeStats[i].loss << "\n";
        }
    }
}

void TournamentResult::push_back(const EngineDuelResult &result)
{
    results_.push_back(result);
}

void TournamentResult::add(const EngineDuelResult& result) {
    // We do not skip if engineB is empty, because this is a wildercard 
    if (result.getEngineA().empty()) {
        return;
    }

    auto it = std::ranges::find_if(results_,
        [&result](const EngineDuelResult& r) {
            return r.engineNamesMatch(result);
        });
    if (it != results_.end()) {
        *it += result;
    }
    else {
        results_.push_back(result);
    }
}

std::vector<std::string> TournamentResult::engineNames() const
{
    std::unordered_set<std::string> names;
    for (const auto &duel : results_)
    {
        names.insert(duel.getEngineA());
        names.insert(duel.getEngineB());
    }
    return {names.begin(), names.end()};
}

std::optional<EngineResult> TournamentResult::forEngine(const std::string &name) const
{
    std::unordered_map<std::string, EngineDuelResult> aggregated;

    for (const auto &duel : results_)
    {
        if (duel.getEngineA() == name || duel.getEngineB() == name)
        {
            EngineDuelResult aligned = (duel.getEngineA() == name) ? duel : duel.switchedSides();

            auto it = aggregated.find(aligned.getEngineB());
            if (it == aggregated.end())
            {
                aggregated[aligned.getEngineB()] = aligned;
            }
            else
            {
                it->second += aligned;
            }
        }
    }

    if (aggregated.empty()) {
        return std::nullopt;
    }

    EngineResult result;
    result.engineName = name;
    for (auto &[_, duel] : aggregated)
    {
        result.duels.push_back(std::move(duel));
    }

    return result;
}

void TournamentResult::initializeScoredEngines(bool update, double baseElo)
{
    if (!update) {
        scoredEngines_.clear();
    }

    std::unordered_map<std::string, Scored*> scoredMap;
    for (auto& scored : scoredEngines_) {
        scoredMap[scored.engineName] = &scored;
    }

    for (const auto& name : engineNames())
    {
        auto opt = forEngine(name);
        if (!opt) {
            continue;
        }

        EngineDuelResult agg = opt->aggregate(name);
        int total = agg.total();
        if (total == 0) {
            continue;
        }

        double score = (agg.winsEngineA + 0.5 * agg.draws) / total;

        auto it = scoredMap.find(name);
        if (it != scoredMap.end()) {
            // Update existing element
            Scored* existing = it->second;
            existing->result = std::move(*opt);
            existing->score = score;
            existing->total = static_cast<double>(total);
        }
        else {
            // Add new element
            scoredEngines_.push_back(Scored{
                .engineName = name,
                .result = std::move(*opt),
                .score = score,
                .elo = baseElo,
                .total = static_cast<double>(total),
                .error = 0
                });
        }
    }
}

double TournamentResult::averageOpponentElo(const Scored &s,
                                            const std::unordered_map<std::string, double> &currentElo)
{
    const std::string &name = s.engineName;
    const auto &duels = s.result.duels;

    double weightedSum = 0.0;
    int totalGames = 0;

    for (const auto &duel : duels)
    {
        std::string opponent = duel.getEngineA() == name ? duel.getEngineB() : duel.getEngineA();
        int games = duel.total();
        if (games == 0) {
            continue;
        }

        weightedSum += currentElo.at(opponent) * games;
        totalGames += games;
    }

    return totalGames > 0 ? weightedSum / totalGames : 0.0;
}

std::vector<TournamentResult::Scored> TournamentResult::computeAllElos(
    int baseElo, int passes, bool update) 
{
    constexpr double convergenceFactor = 0.5;
    constexpr double weightConstant = 200.0;

    initializeScoredEngines(update, baseElo);
    std::unordered_map<std::string, Scored*> scoredMap;
    for (auto& s : scoredEngines_) {
        scoredMap[s.engineName] = &s;
    }

    for (int pass = 0; pass < passes; ++pass) {

        for (auto& s : scoredEngines_) {
            const std::string& name = s.engineName;

            for (const auto& duel : s.result.duels) {
                if (duel.getEngineA() != name) {
                    continue;
                }
                std::string opponent = duel.getEngineB();
                auto it = scoredMap.find(opponent);
                if (it == scoredMap.end()) {
                    continue;
                }
                Scored* opponentScore = it->second;

                int total = duel.total();
                if (total == 0) {
                    continue;
                }

                int targetEloDiff = computeEloWithError(duel.winsEngineA, duel.winsEngineB, duel.draws).first;
                double currentEloDiff = s.elo - opponentScore->elo;
                double neededDelta = static_cast<double>(targetEloDiff) - currentEloDiff;

                double weight = static_cast<double>(total) / (total + weightConstant);
                double delta = weight * convergenceFactor * neededDelta;

                s.elo    += delta * 0.5;
                opponentScore->elo -= delta * 0.5;
                
            }
        }
    }

    for (auto& s : scoredEngines_) {
        const auto& agg = s.result.aggregate(s.engineName);
        s.error = computeEloWithError(agg.winsEngineA, agg.winsEngineB, agg.draws).second;
    }

    std::ranges::sort(scoredEngines_, [](const Scored& a, const Scored& b) {
        return a.elo > b.elo;
    });

    return scoredEngines_;
}


void TournamentResult::printRatingTableUciStyle(std::ostream &os, int averageElo) 
{
    os << "\nTournament result:\n";

    std::vector<Scored> list = computeAllElos(averageElo);

    int rank = 1;
    for (const auto &entry : list)
    {
        const auto &r = entry.result.aggregate(entry.engineName);
        const int total = r.total();
        double drawPct = 100.0 * r.draws / total;
        double scorePct = 100.0 * entry.score;
        os << std::left << std::fixed << std::setprecision(1)
           << "rank " << std::setw(3) << rank++
           << " name " << std::setw(25) << entry.engineName;

        if (total < 10 || entry.error == 0)
        {
            os << " not enough games\n";
            continue;
        }
        std::stringstream oss;
        oss << std::fixed << std::setprecision(2) << scorePct << "%";

        os << " elo " << std::setw(5) << entry.elo
           << " +/- " << std::setw(4) << entry.error
           << " games " << std::setw(3) << total
           << " score " << std::setw(7) << oss.str()
           << " draw "  << drawPct << "%\n";
    }
    os << "\n" << std::flush;
}

void TournamentResult::printOutcome(std::ostream &os) const
{
    os << "\nTournament outcome:\n";

    for (const auto &name : engineNames())
    {
        auto optResult = forEngine(name);
        if (!optResult) {
            continue;
        }

        optResult->printOutcome(os);
    }
    os << "\n" << std::flush;
}

std::vector<std::vector<std::string>> TournamentResult::getSummary() const
{
    std::vector<std::vector<std::string>> lines;
    lines.reserve(engineNames().size());

    for (const auto& name : engineNames())
    {
        auto optResult = forEngine(name);
        if (!optResult) {
            continue;
        }

        EngineDuelResult agg = optResult->aggregate(name);
        int wins = agg.winsEngineA;
        int losses = agg.winsEngineB;
        int draws = agg.draws;
        int total = wins + losses + draws;

        double score = total > 0 ? (wins + 0.5 * draws) / total : 0.0;
        std::vector<std::string> row;
        
        row.push_back(name); 
        row.push_back(std::format("{:.1f}%", score * 100));
        row.push_back(std::to_string(wins)); 
        row.push_back(std::to_string(draws));
        row.push_back(std::to_string(losses));

        lines.emplace_back(row);
    }

    std::ranges::sort(lines, [](const auto& a, const auto& b)
        {
            return b[1] < a[1]; // descending
        });

    return lines;

}

void TournamentResult::printSummary(std::ostream &os) const
{
    os << "\nTournament result:\n";

    auto lines = getSummary();
    for (const auto& entry : lines)
    {
        os << std::left << std::fixed << std::setprecision(2)
            << std::setw(30) << entry[0] // Name
            << " " << entry[1]           // Score
            << "  W:" << entry[2]        // Wins
            << " D:" << entry[3]         // Draws
            << " L:" << entry[4]         // Losses
            << "\n";
    }
    os << "\n" << std::flush;
}

} // namespace QaplaTester
