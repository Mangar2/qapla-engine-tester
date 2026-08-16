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
#include "../base-elements/elo-helper.h"
#include "../sprt/sprt-calculation.h"

#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <format>

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

bool EngineDuelResult::addPentanomialResult(GameResult result1, GameResult result2, bool switchColors) {
    if (result1 == GameResult::Unterminated || result2 == GameResult::Unterminated) {
        return false;
    }

    bool aWon1 = (result1 == GameResult::WhiteWins);
    bool draw1 = (result1 == GameResult::Draw);
    bool aLost1 = (result1 == GameResult::BlackWins);

    bool aWon2 = (result2 == (switchColors ? GameResult::BlackWins : GameResult::WhiteWins));
    bool draw2 = (result2 == GameResult::Draw);
    bool aLost2 = (result2 == (switchColors ? GameResult::WhiteWins : GameResult::BlackWins));

    if (aWon1 && aWon2) {
        ++pentaWW;
    } else if ((aWon1 && draw2) || (draw1 && aWon2)) {
        ++pentaWD;
    } else if ((aWon1 && aLost2) || (aLost1 && aWon2)) {
        ++pentaWL;
    } else if (draw1 && draw2) {
        ++pentaDD;
    } else if ((aLost1 && draw2) || (draw1 && aLost2)) {
        ++pentaLD;
    } else if (aLost1 && aLost2) {
        ++pentaLL;
    }

    return true;
}

EngineDuelResult EngineDuelResult::switchedSides() const
{
    EngineDuelResult result(engineB, engineA);
    result.winsEngineA = winsEngineB;
    result.winsEngineB = winsEngineA;
    result.draws = draws;

    // Swap pentanomial stats (WW becomes LL, etc.)
    result.pentaWW = pentaLL;
    result.pentaWD = pentaLD;
    result.pentaWL = pentaWL;  // WL is symmetric
    result.pentaDD = pentaDD;  // DD is symmetric
    result.pentaLD = pentaWD;
    result.pentaLL = pentaWW;

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
        pentaWW += other.pentaWW;
        pentaWD += other.pentaWD;
        pentaWL += other.pentaWL;
        pentaDD += other.pentaDD;
        pentaLD += other.pentaLD;
        pentaLL += other.pentaLL;
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
        pentaWW += other.pentaLL;
        pentaWD += other.pentaLD;
        pentaWL += other.pentaWL;
        pentaDD += other.pentaDD;
        pentaLD += other.pentaWD;
        pentaLL += other.pentaWW;
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
    // Damped Fisher scoring: a full step (1.0) can overshoot when an engine has only a
    // handful of games, half a step converges to well below display precision within ~20
    // passes while staying stable.
    constexpr double damping = 0.5;
    // d/dElo of the logistic expected score, without the p*(1-p) factor.
    const double logisticSlope = std::log(10.0) / 400.0;

    initializeScoredEngines(update, baseElo);
    std::unordered_map<std::string, Scored*> scoredMap;
    for (auto& s : scoredEngines_) {
        scoredMap[s.engineName] = &s;
    }

    // Fits each engine's rating so that its expected score against the current opponent
    // ratings matches the points it actually scored (maximum likelihood over the logistic
    // model, i.e. the same model SPRT tests against -- see SprtBase::logisticScore).
    //
    // Deliberately compares points rather than per-duel Elo differences: Elo is a
    // non-linear function of the score, so averaging per-duel Elo values does not
    // reproduce the Elo of the summed score. That is what previously allowed an engine to
    // score more points than another against an identical opponent field and still be
    // rated lower. Matching points instead makes the expected score strictly increasing in
    // the engine's own rating, so with an identical opponent field more points must yield
    // more Elo. It also gives the iteration a single fixed point, which the previous
    // pairwise nudging did not have -- results were sensitive to the order in which duels
    // happened to be visited.
    //
    // Still fully incremental: one pass costs the same as before (one visit per duel), and
    // continuing from existing ratings after a newly finished game converges within a
    // pass or two.
    for (int pass = 0; pass < passes; ++pass) {

        for (auto& s : scoredEngines_) {
            double actualPoints = 0.0;
            double expectedPoints = 0.0;
            double slope = 0.0;

            for (const auto& duel : s.result.duels) {
                if (duel.getEngineA() != s.engineName) {
                    continue;
                }
                auto it = scoredMap.find(duel.getEngineB());
                if (it == scoredMap.end()) {
                    continue;
                }

                const int total = duel.total();
                if (total == 0) {
                    continue;
                }

                const double expectedScore = SprtBase::logisticScore(s.elo - it->second->elo);
                actualPoints += duel.winsEngineA + 0.5 * duel.draws;
                expectedPoints += total * expectedScore;
                slope += total * logisticSlope * expectedScore * (1.0 - expectedScore);
            }

            if (slope > 1e-12) {
                s.elo += damping * (actualPoints - expectedPoints) / slope;
            }
        }

        // The likelihood only constrains rating *differences*, so the whole scale would
        // drift freely; anchoring the mean keeps it on the caller's baseElo.
        if (!scoredEngines_.empty()) {
            double sum = 0.0;
            for (const auto& s : scoredEngines_) {
                sum += s.elo;
            }
            const double shift = baseElo - sum / static_cast<double>(scoredEngines_.size());
            for (auto& s : scoredEngines_) {
                s.elo += shift;
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

Json::JsonValue TournamentResult::getRatingTable(int averageElo) {
    std::vector<Scored> list = computeAllElos(averageElo);

    auto root = Json::JsonValue::object();
    root["type"] = "ratingTable";
    auto& data = root["data"] = Json::JsonValue::array();

    int rank = 1;
    for (const auto& entry : list) {
        const auto& r = entry.result.aggregate(entry.engineName);
        const int total = r.total();
        const double drawPct = total > 0 ? 100.0 * r.draws / total : 0.0;
        const double scorePct = 100.0 * entry.score;

        auto& row = data[data.size()];
        row["rank"] = static_cast<double>(rank++);
        row["name"] = entry.engineName;
        row["elo"] = std::round(entry.elo * 10.0) / 10.0;
        row["error"] = static_cast<double>(entry.error);
        row["games"] = static_cast<double>(total);
        row["score"] = std::round(scorePct * 100.0) / 100.0;
        row["drawPct"] = std::round(drawPct * 10.0) / 10.0;
    }

    return root;
}

Json::JsonValue TournamentResult::getOutcome() const {
    auto root = Json::JsonValue::object();
    root["type"] = "outcome";
    auto& data = root["data"] = Json::JsonValue::array();

    for (const auto& name : engineNames()) {
        auto optResult = forEngine(name);
        if (!optResult) {
            continue;
        }

        const auto& agg = optResult->aggregate(name);
        auto& row = data[data.size()];
        row["name"] = name;
        row["wins"] = static_cast<double>(agg.winsEngineA);
        row["losses"] = static_cast<double>(agg.winsEngineB);
        row["draws"] = static_cast<double>(agg.draws);
        row["total"] = static_cast<double>(agg.total());
    }

    return root;
}

} // namespace QaplaTester
