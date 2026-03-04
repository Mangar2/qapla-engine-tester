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

#include "adjudication-config.h"

namespace QaplaTester {

AdjudicationManager::DrawAdjudicationConfig AdjudicationConfig::fromDrawManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto draw = manager.getGroupInstance(groupName);
    if (!draw) {
        return AdjudicationManager::DrawAdjudicationConfig{};
    }

    return AdjudicationManager::DrawAdjudicationConfig{
        .minFullMoves = draw->get<unsigned int>("movenumber"),
        .requiredConsecutiveMoves = draw->get<unsigned int>("movecount"),
        .centipawnThreshold = draw->get<int>("score"),
        .testOnly = draw->get<bool>("test"),
        .active = true
    };
}

AdjudicationManager::ResignAdjudicationConfig AdjudicationConfig::fromResignManager(
    Settings::Manager& manager,
    const std::string& groupName) {
    
    auto resign = manager.getGroupInstance(groupName);
    if (!resign) {
        return AdjudicationManager::ResignAdjudicationConfig{};
    }

    return AdjudicationManager::ResignAdjudicationConfig{
        .requiredConsecutiveMoves = resign->get<unsigned int>("movecount"),
        .centipawnThreshold = resign->get<int>("score"),
        .testOnly = resign->get<bool>("test"),
        .twoSided = resign->get<bool>("twosided"),
        .active = true
    };
}

} // namespace QaplaTester
