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

#include "pgn-config.h"
#include "../base-elements/app-error.h"

#include <format>

namespace QaplaTester {

PgnSave::Options PgnConfig::fromManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto pgnInstance = manager.getGroupInstance(groupName);
    if (!pgnInstance) {
        return PgnSave::Options{};
    }

    const auto& pgn = *pgnInstance;
    const auto notation = pgn.get<std::string>("notation");
    if (notation != "san" && notation != "lan") {
        throw AppError::makeInvalidParameters(std::format(
            "Set {}.notation to 'san' or 'lan'; '{}' is neither.", groupName, notation));
    }
    return PgnSave::Options{
        .file = pgn.get<std::string>("file"),
        .append = pgn.get<bool>("append"),
        .onlyFinishedGames = pgn.get<bool>("finished"),
        .minimalTags = pgn.get<bool>("min"),
        .includeClock = pgn.get<bool>("clock"),
        .includeEval = pgn.get<bool>("eval"),
        .includePv = pgn.get<bool>("pv"),
        .includeDepth = pgn.get<bool>("depth"),
        .lan = notation == "lan"
    };
}

} // namespace QaplaTester
