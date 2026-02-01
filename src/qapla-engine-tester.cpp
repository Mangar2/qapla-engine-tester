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
 * @author Volker BÃ¶hm
 * @copyright Copyright (c) 2025 Volker BÃ¶hm
 */

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <format>
#include <sstream>

#ifndef _WIN32
#include <signal.h>
#endif

#include "engine-tester/engine-report.h"
#include "engine-tester/engine-test-controller.h"
#include "engine-handling/engine-worker-factory.h"
#include "epd/epd-manager.h"
#include "sprt/sprt-manager.h"
#include "sprt/sprt-tournament-file.h"
#include "spsa/spsa-optimizer.h"
#include "tournament/tournament.h"
#include "tournament/tournament-file.h"
#include "opening/pgn-save.h"

#include "cli/input-handler.h"
#include "cli/settings-manager.h"
#include "cli/qapla-settings.h"
#include "cli/app-runner.h"

#include "game-manager/game-manager-pool.h"
#include "game-manager/adjudication-manager.h"

#include "base-elements/app-error.h"
#include "base-elements/timer.h"
#include "base-elements/time-control.h"
#include "base-elements/logger.h"
#include "mcp/mcp-server.h"

using namespace QaplaTester;
using QaplaHelpers::Timer;

static AppReturnCode run() {
    if (Settings::Manager::instance().get<bool>("mcp")) {
        return Mcp::McpServer::run();
    }

    AppReturnCode returnCode = AppReturnCode::NoError;

    InputHandler::inputLoop(
        Settings::QaplaSettings::instance().getArguments().size() == 1 
        || Settings::Manager::instance().get<bool>("interactive"));

    AppRunner::setPgnConfig();
    AppRunner::setAdjudicationOptions();

    if (auto test = Settings::Manager::instance().getGroupInstance("test")) {
        returnCode = AppRunner::runTest(*test, returnCode);
    }

    if (Settings::QaplaSettings::instance().getEpdConfig()) {
        returnCode = AppRunner::runEpd(returnCode);
    }

    if (Settings::QaplaSettings::instance().getTournamentConfig()) {
        returnCode = AppRunner::runTournament(returnCode);
    }

    if (Settings::QaplaSettings::instance().getSprtConfig()) {
        returnCode = AppRunner::runSprt(returnCode);
    }

    if (Settings::QaplaSettings::instance().getSPSAConfig()) {
        returnCode = AppRunner::runSpsa(returnCode);
    }
    return returnCode;
}

int main(int argc, char** argv) {
    #ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    #endif
    Timer timer;
    timer.start();
    

    AppReturnCode returnCode = AppReturnCode::NoError;
    try {
        // Initialize settings
        Settings::QaplaSettings::init();
        
        // Convert and store arguments
        std::vector<std::string> args;
        args.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        Settings::QaplaSettings::instance().applyArguments(args);

        returnCode = run();
			
    }
    catch (const AppError& ex) {
		Logger::reportLogger().log(std::format("Error: {}", ex.what()), TraceLevel::error);
        returnCode = ex.getReturnCode();
    }
	catch (const std::exception& e) {
		Logger::reportLogger().log(std::format("{}", e.what()), TraceLevel::error);
        returnCode = AppReturnCode::GeneralError;
	}
	catch (...) {
		Logger::reportLogger().log("Unknown exception, program terminated.", TraceLevel::error);
		returnCode = AppReturnCode::GeneralError;
	}
	Logger::reportLogger().log(timer.getElapsedString("Total runtime"));
	
    // Unregisters the input handler callback before destruction of the input handler
	GameManagerPool::resetInstance();
    return static_cast<int>(returnCode);
}

