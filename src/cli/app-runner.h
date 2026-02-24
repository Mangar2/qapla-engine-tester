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
#include "task-types.h"

#include <memory>
#include <atomic>

namespace QaplaTester {

class Tournament;
class EpdManager;
class SprtManager;
class SPSAOptimizer;
class SystemTestManager;

/**
 * @brief Handles the execution of different application modes.
 */
class AppRunner {
public:
    static AppRunner& instance() {
        static AppRunner instance;
        return instance;
    }

    AppRunner(const AppRunner&) = delete;
    AppRunner& operator=(const AppRunner&) = delete;

    /**
     * @brief Runs the engine testing mode.
     * @param test The settings for the test.
     * @param code Current application return code.
     * @return Updated application return code.
     */
    [[nodiscard]] static AppReturnCode runTest(const Settings::GroupInstance& test, AppReturnCode code);

    /**
     * @brief Runs the EPD test mode.
     * @param code Current application return code.
     * @param background If true, starts in background and returns immediately.
     * @return Updated application return code.
     */
    [[nodiscard]] AppReturnCode runEpd(AppReturnCode code, bool background = false);

    /**
     * @brief Runs the tournament mode.
     * @param code Current application return code.
     * @param background If true, starts in background and returns immediately.
     * @return Updated application return code.
     */
    [[nodiscard]] AppReturnCode runTournament(AppReturnCode code, bool background = false);

    /**
     * @brief Runs the SPRT mode.
     * @param code Current application return code.
     * @param background If true, starts in background and returns immediately.
     * @return Updated application return code.
     */
    [[nodiscard]] AppReturnCode runSprt(AppReturnCode code, bool background = false);

    /**
     * @brief Runs the SPSA mode.
     * @param code Current application return code.
     * @param background If true, starts in background and returns immediately.
     * @return Updated application return code.
     */
    [[nodiscard]] AppReturnCode runSpsa(AppReturnCode code, bool background = false);

    /**
     * @brief Runs the system-test mode.
     * @param code Current application return code.
     * @param background If true, starts in background and returns immediately.
     * @return Updated application return code.
     */
    [[nodiscard]] AppReturnCode runSystemTest(AppReturnCode code, bool background = false);

    /**
     * @brief Sets adjudication options based on settings.
     */
    static void setAdjudicationOptions();

    /**
     * @brief Sets PGN configuration based on settings.
     */
    static void setPgnConfig();

    /**
     * @brief Dispatches execution to the correct mode based on current settings.
     * @param background If true, starts in background and returns immediately.
     * @param forcedTask If set, only this task is executed and all other configured tasks are ignored.
     * @return Application return code.
     */
    [[nodiscard]] AppReturnCode runDispatcher(bool background = false,
        Cli::TaskType forcedTask = Cli::TaskType::None);

    /**
     * @brief Stops all running tasks.
     * @param nice If true, stops gracefully by setting concurrency to 0.
     */
    static void stop(bool nice = false);

    /**
     * @brief Sets the concurrency level for the game manager pool.
     * @param value Concurrency value.
     */
    static void setConcurrency(int value);

    /**
     * @brief Returns the status of the runner (running games count).
     */
    static std::string getRunningGameCount();

    /**
     * @brief Returns the status of the runner including current task details.
     */
    static std::string getStatus();

    /**
     * @brief Returns the status payload for one task as serialized JSON object.
     * @param taskType The task to query.
     */
    static std::string getTaskStatusJson(Cli::TaskType taskType);

    [[nodiscard]] std::shared_ptr<Tournament> getTournament() const { return tournament_; }
    [[nodiscard]] std::shared_ptr<EpdManager> getEpdManager() const { return epdManager_; }
    [[nodiscard]] std::shared_ptr<SprtManager> getSprtManager() const { return sprtManager_; }
    [[nodiscard]] std::shared_ptr<SPSAOptimizer> getSPSAOptimizer() const { return spsaOptimizer_; }
    [[nodiscard]] std::shared_ptr<SystemTestManager> getSystemTestManager() const { return systemTestManager_; }

private:
    AppRunner() = default;

    std::shared_ptr<Tournament> tournament_;
    std::shared_ptr<EpdManager> epdManager_;
    std::shared_ptr<SprtManager> sprtManager_;
    std::shared_ptr<SPSAOptimizer> spsaOptimizer_;
    std::shared_ptr<SystemTestManager> systemTestManager_;

    std::atomic<Cli::TaskType> currentTask_ = Cli::TaskType::None;
};

} // namespace QaplaTester
