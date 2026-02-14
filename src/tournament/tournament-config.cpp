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

#include "tournament-config.h"
#include "../cli/settings-manager.h"

namespace QaplaTester {


TournamentConfig TournamentConfigFile::fromManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto tournament = manager.getGroupInstance(groupName);
    if (!tournament) {
        return TournamentConfig{};
    }

    return TournamentConfig{
        .event = tournament->get<std::string>("event"),
        .type = tournament->get<std::string>("type"),
        .tournamentFilename = tournament->get<std::string>("file"),
        .saveIntervalMs = tournament->get<unsigned int>("saveintervalS") * 1000,
        .games = tournament->get<unsigned int>("games"),
        .rounds = tournament->get<unsigned int>("rounds"),
        .repeat = tournament->get<unsigned int>("repeat"),
        .ratingInterval = tournament->get<unsigned int>("ratinginterval"),
        .outcomeInterval = tournament->get<unsigned int>("outcomeinterval"),
        .averageElo = tournament->get<int>("averageelo"),
        .noSwap = tournament->get<bool>("noswap"),
        .openings = Openings{}
    };
}

} // namespace QaplaTester
