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

#include "opening-config.h"
#include "../base-elements/app-error.h"

namespace QaplaTester {

Openings OpeningConfig::fromManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto opening = manager.getGroupInstance(groupName);
    if (!opening) {
        return Openings{};
    }

    const auto pliesStr = opening->get<std::string>("plies");
    std::optional<int> plies;

    if (pliesStr != "all") {
        try {
            int val = std::stoi(pliesStr);
            if (val < 0) {
                throw AppError::makeInvalidParameters("Openings: Ply count must be at least 0, but got " + pliesStr);
            }
            plies = val - 1;
        }
        catch (const std::exception&) {
            throw AppError::makeInvalidParameters(
                "Openings: Ply count must be a non-negative integer or \"all\", but got: \"" + pliesStr + "\"");
        }
    }

    Openings openings{
        .file = opening->get<std::string>("file"),
        .order = opening->get<std::string>("order"),
        .plies = plies,
        .start = opening->get<unsigned int>("start"),
        .seed = opening->get<unsigned int>("srand"),
        .policy = opening->get<std::string>("policy")
    };

    if (openings.start < 1) {
        throw AppError::makeInvalidParameters("Openings: Start index must be at least 1, but got " +
            std::to_string(openings.start));
    }
    openings.start--;

    if (openings.order != "sequential" && openings.order != "random") {
        throw AppError::makeInvalidParameters("Unsupported openings order: " + openings.order);
    }
    if (openings.policy != "default" && openings.policy != "encounter" && openings.policy != "round") {
        throw AppError::makeInvalidParameters("Unsupported openings policy: " + openings.policy);
    }

    return openings;
}

} // namespace QaplaTester
