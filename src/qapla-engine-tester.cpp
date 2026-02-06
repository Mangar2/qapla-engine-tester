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



#include "engine-tester/epd-test.h"

#include "sprt/sprt-tournament-file.h"
#include "spsa/spsa-optimizer.h"
#include "tournament/tournament-file.h"

#include "cli/input-handler.h"
#include "cli/settings-manager.h"
#include "cli/qapla-settings.h"
#include "cli/app-runner.h"

#include "game-manager/game-manager-pool.h"

#include "base-elements/app-error.h"
#include "base-elements/timer.h"
#include "base-elements/logger.h"
#include "mcp/mcp-server.h"

#include <string>
#include <vector>
#include <format>

#ifndef _WIN32
#include <signal.h>
#endif

using namespace QaplaTester;
using QaplaHelpers::Timer;

static AppReturnCode run() {
    if (Settings::Manager::instance().getGroupInstance("mcp")) {
        return Mcp::McpServer::run();
    }

    InputHandler::inputLoop(
        Settings::QaplaSettings::instance().getArguments().size() == 1 
        || Settings::Manager::instance().get<bool>("interactive"));

    return AppRunner::runDispatcher();
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
        Settings::QaplaSettings::instance().initializeConfigs(args);

        if (!Settings::Manager::instance().getGroupInstance("mcp")) {
            Logger::reportLogger().logCli(Logger::getWelcomeMessage());
        }

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

