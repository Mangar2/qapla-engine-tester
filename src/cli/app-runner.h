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

#include "../base-elements/app-error.h"
#include "settings-manager.h"

namespace QaplaTester {

/**
 * @brief Handles the execution of different application modes.
 */
class AppRunner {
public:
    /**
     * @brief Runs the engine testing mode.
     * @param test The settings for the test.
     * @param code Current application return code.
     * @return Updated application return code.
     */
    static AppReturnCode runTest(const Settings::GroupInstance& test, AppReturnCode code);

    /**
     * @brief Runs the EPD test mode.
     * @param code Current application return code.
     * @return Updated application return code.
     */
    static AppReturnCode runEpd(AppReturnCode code);

    /**
     * @brief Runs the tournament mode.
     * @param code Current application return code.
     * @return Updated application return code.
     */
    static AppReturnCode runTournament(AppReturnCode code);

    /**
     * @brief Runs the SPRT mode.
     * @param code Current application return code.
     * @return Updated application return code.
     */
    static AppReturnCode runSprt(AppReturnCode code);

    /**
     * @brief Runs the SPSA mode.
     * @param code Current application return code.
     * @return Updated application return code.
     */
    static AppReturnCode runSpsa(AppReturnCode code);

    /**
     * @brief Sets adjudication options based on settings.
     */
    static void setAdjudicationOptions();

    /**
     * @brief Sets PGN configuration based on settings.
     */
    static void setPgnConfig();
};

} // namespace QaplaTester
