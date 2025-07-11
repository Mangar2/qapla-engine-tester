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

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <iostream>

#include "app-error.h"
#include "engine-report.h"
#include "engine-test-controller.h"
#include "logger.h"
#include "engine-worker-factory.h"
#include "cli-settings-manager.h"
#include "epd-manager.h"
#include "sprt-manager.h"
#include "tournament.h"
#include "timer.h"
#include "time-control.h"
#include "pgn-io.h"
#include "input-handler.h"
#include "game-manager-pool.h"

auto updateCode(AppReturnCode code, AppReturnCode newCode) {
	if (code == AppReturnCode::NoError) {
		return newCode;
	}
	else if (code >= AppReturnCode::EngineError) {
		return std::min(code, newCode);
	}
	return code;
}

auto logChecklist(AppReturnCode code, TraceLevel traceLevel = TraceLevel::command) {
    auto newCode = EngineReport::logAll(traceLevel);
    if (code == AppReturnCode::NoError) {
        code = newCode;
    }
    else if (code >= AppReturnCode::EngineError) {
        code = std::min(code, newCode);
    }
    return code;
}

auto runEpd(const CliSettings::GroupInstances& epdList, AppReturnCode code) {
	int concurrency = CliSettings::Manager::get<int>("concurrency");
    Logger::testLogger().setLogFile("epd-report");
    Logger::testLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);
    EpdManager epdManager;
	for (auto& epd : epdList) {
        std::string file;
        int maxTime = 10;
        int minTime = 2;
        int seenPlies = 3;
		file = epd.get<std::string>("file");
		maxTime = epd.get<int>("maxtime");
		minTime = epd.get<int>("mintime");
		seenPlies = epd.get<int>("seenplies");

		for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
            std::string name = engine.getName();
            std::string earlyStop = minTime < 0 ? "" : "Early stop - Seen plies: " + std::to_string(seenPlies) + " Min time: " + std::to_string(minTime) + "s";
			Logger::testLogger().log("Using engine: " + name 
                + " Concurrency: " + std::to_string(concurrency) + " Max Time: " + std::to_string(maxTime) + "s "
                + earlyStop);
            epdManager.analyzeEpd(file, engine, concurrency, maxTime, minTime, seenPlies);
            epdManager.wait();
			code = logChecklist(code, TraceLevel::info);
			auto minSuccess = epd.get<int>("minsuccess");
            if (code == AppReturnCode::NoError || code == AppReturnCode::EngineNote) {
                bool success = epdManager.getSuccessRate() >= minSuccess / 100.0;
				code = success ? code : AppReturnCode::MissedTarget;
            }
		}
	}
    return code;
}

AppReturnCode handleGlobalOptions(AppReturnCode code) {
    if (!CliSettings::Manager::get<std::string>("logpath").empty()) {
        Logger::setLogPath(CliSettings::Manager::get<std::string>("logpath"));
    }
    if (CliSettings::Manager::get<bool>("enginelog")) {
        Logger::engineLogger().setLogFile("qapla-engine-trace");
        Logger::engineLogger().setTraceLevel(TraceLevel::error, TraceLevel::info);
    }

    return code;
}

AppReturnCode runTest(const CliSettings::GroupInstance& test, AppReturnCode code) {
    Logger::testLogger().setLogFile("engine-report");
    Logger::testLogger().setTraceLevel(TraceLevel::warning);
    if (!Logger::engineLogger().getFilename().empty()) {
        Logger::testLogger().logAligned("Engine communication log: ", Logger::engineLogger().getFilename());
    }
    Logger::testLogger().logAligned("Summary test report log: ", Logger::testLogger().getFilename());

    EngineTestController controller;
    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        std::string name = engine.getName();
        try {
			EngineReport::reportUnderruns = test.get<bool>("underrun");
            controller.runAllTests(engine, test.get<int>("numgames"));
        }
        catch (const AppError& ex) {
            Logger::testLogger().log("Application error during engine test for " + name + ": " + std::string(ex.what()), 
                TraceLevel::error);
            code = ex.getReturnCode();
        }
		catch (const std::exception& e) {
			Logger::testLogger().log("Application error during engine test for " + name + ": " + std::string(e.what()), 
                TraceLevel::error);
            code = AppReturnCode::GeneralError;
		}
		catch (...) {
			Logger::testLogger().log("Unknown exception during engine test for " + name, TraceLevel::error);
            code = AppReturnCode::GeneralError;
		}
        code = logChecklist(code);
    }
    return code;
}

std::optional<Openings> readOpenings() {
    auto opening = CliSettings::Manager::getGroupInstance("openings");
    if (!opening) {
        Logger::testLogger().log("No openings defined. Please define an opening, see --help for more info.", TraceLevel::error);
		return std::nullopt;
    }

	const auto pliesStr = opening->get<std::string>("plies");
    std::optional<int> plies;

    if (pliesStr != "all") {
        try {
            int val = std::stoi(pliesStr);
            if (val < 0) {
                throw AppError::makeInvalidParameters("Openings: Ply count must be at least 0, but got " + pliesStr);
            }
            plies = val - 1; // intern 0-basiert
        }
        catch (const std::exception&) {
            throw AppError::makeInvalidParameters(
                "Openings: Ply count must be a non-negative integer or \"all\", but got: \"" + pliesStr + "\""); 
        }
    }

    if (opening->get<int>("srand") < 0) {
        throw AppError::makeInvalidParameters("Openings: Seed must be a non-negative integer, but got " +
            std::to_string(opening->get<int>("srand")));
    }

    Openings openings{
        .file = opening->get<std::string>("file"),
        .format = opening->get<std::string>("format"),
        .order = opening->get<std::string>("order"),
        .plies = plies,
        .start = opening->get<int>("start") - 1, // 1 based index in gui.
        .seed = static_cast<uint32_t>(opening->get<int>("srand")),
        .policy = opening->get<std::string>("policy")
    };
    if (openings.start < 0) {
        throw AppError::makeInvalidParameters("Openings: Start index must be at least 1, but got " +
            std::to_string(openings.start + 1));
    }
    if (openings.format != "epd" && openings.format != "raw" && openings.format != "pgn") {
		throw AppError::makeInvalidParameters("Unsupported openings format: " + openings.format);
    }
    if (openings.order != "sequential" && openings.order != "random") {
        throw AppError::makeInvalidParameters("Unsupported openings order: " + openings.order);
    }
    if (openings.policy != "default" && openings.policy != "encounter" && openings.policy != "round") {
        throw AppError::makeInvalidParameters("Unsupported openings policy: " + openings.policy);
    }
    return openings;
}

void checkTimeControl() {
    for (const auto& engine : EngineWorkerFactory::getActiveEngines()) {
        if (!engine.getTimeControl().isValid()) {
            throw AppError::makeInvalidParameters("No valid time control defined for engine '" + engine.getName()
                + "'. Please specify a time control using 'tc' option.");
        }
    }
} 

auto runSprt(AppReturnCode code) {
    auto sprt = CliSettings::Manager::getGroupInstance("sprt");
	auto opening = CliSettings::Manager::getGroupInstance("openings");  
    Openings openings;
    if (!sprt) return code;
    auto isMontecarlo = sprt->get<bool>("montecarlo");
    if (!opening && !isMontecarlo) {
		Logger::testLogger().log("No openings defined for SPRT tests. Please define an opening, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    else {
		openings = *readOpenings();
    }
	auto activeEngines = EngineWorkerFactory::getActiveEngines();
    if (activeEngines.size() < 2 && !isMontecarlo) {
        Logger::testLogger().log("At least two engines must be defined for SPRT tests. Please define two engines, see --help for more info.",
            TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    checkTimeControl();
    Logger::testLogger().setLogFile("sprt-report");
    Logger::testLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);
    try {
        SprtConfig config{
            .eloUpper = sprt->get<int>("eloUpper"),
            .eloLower = sprt->get<int>("eloLower"),
            .alpha = sprt->get<float>("alpha"),
            .beta = sprt->get<float>("beta"),
            .maxGames = sprt->get<int>("maxgames"),
            .openings = openings
        };
        int concurrency = CliSettings::Manager::get<int>("concurrency");

        SprtManager manager;
		if (isMontecarlo) {
            manager.runMonteCarloTest(config);
		}
        else {
            auto filename = sprt->get<std::string>("resultfile");
            manager.createTournament(activeEngines[0], activeEngines[1], config);
            manager.load(filename);
            manager.schedule(concurrency);
            manager.wait();
            if (!filename.empty()) {
                manager.save(filename);
            }
			code = updateCode(code, EngineReport::logAll(TraceLevel::command, manager.getResult()));
			Logger::testLogger().log("sprt all games completed", TraceLevel::result);

            if (code == AppReturnCode::NoError || code == AppReturnCode::EngineNote) {
                auto decision = manager.getDecision();
				code = !decision ? AppReturnCode::UndefinedResult : 
                    (*decision ? AppReturnCode::H1Accepted : AppReturnCode::H0Accepted);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during sprt run: " + std::string(e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during sprt run: ", TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    return code;
}

AppReturnCode runTournament(AppReturnCode code) {
    auto tournamentGroup = CliSettings::Manager::getGroupInstance("tournament");

    if (!tournamentGroup) return code;

    auto activeEngines = EngineWorkerFactory::getActiveEngines();
    if (activeEngines.size() < 2) {
        Logger::testLogger().log("At least two engines must be defined. Please define more engines, see --help for more info.", TraceLevel::error);
        return AppReturnCode::InvalidParameters;
    }
    checkTimeControl();

    std::optional<Openings> openings = readOpenings();
    if (!openings) {
        return AppReturnCode::InvalidParameters;
    }

    Logger::testLogger().setLogFile("tournament-report");
    Logger::testLogger().setTraceLevel(TraceLevel::result, TraceLevel::result);

    try {

        TournamentConfig config = {
            .event = tournamentGroup->get<std::string>("event"),
            .type = tournamentGroup->get<std::string>("type"),
            .games = tournamentGroup->get<int>("games"),
            .rounds = tournamentGroup->get<int>("rounds"),
            .repeat = tournamentGroup->get<int>("repeat"),
            .ratingInterval = tournamentGroup->get<int>("ratinginterval"),
            .averageElo = tournamentGroup->get<int>("averageelo"),
            .outcomeInterval = tournamentGroup->get<int>("outcomeinterval"),
            .noSwap = tournamentGroup->get<bool>("noswap"),
            .openings = *openings
        };

        int concurrency = CliSettings::Manager::get<int>("concurrency");

        Tournament tournament;
        auto filename = tournamentGroup->get<std::string>("resultfile");
        tournament.createTournament(activeEngines, config);
		tournament.load(filename);
        tournament.scheduleAll(concurrency);
        tournament.wait();
		if (!filename.empty()) {
			tournament.save(filename);
		}
        Logger::testLogger().log("tournament all games completed", TraceLevel::result);
        std::string resultString = tournament.getResultString();
        Logger::testLogger().log(resultString, TraceLevel::result);

 		code = updateCode(code, EngineReport::logAll(TraceLevel::info, tournament.getResult()));
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during tournament run: " + std::string(e.what()), TraceLevel::error);
        return AppReturnCode::GeneralError;
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during tournament run.", TraceLevel::error);
        return AppReturnCode::GeneralError;
    }

    return code;
}


void handlePgnOptions() {
    auto pgnOptionInstance = CliSettings::Manager::getGroupInstance("pgnoutput");
    if (!pgnOptionInstance) return;

    auto pgn = *pgnOptionInstance;
    PgnIO::Options pgnOptions{
        .file = pgn.get<std::string>("file"),
        .append = pgn.get<bool>("append"),
        .onlyFinishedGames = pgn.get<bool>("fi"),
        .minimalTags = pgn.get<bool>("min"),
        //.saveAfterMove = pgn.get<bool>("aftermove"),
        .includeClock = pgn.get<bool>("clock"),
        .includeEval = pgn.get<bool>("eval"),
        .includePv = pgn.get<bool>("pv"),
        .includeDepth = pgn.get<bool>("depth")
    };
    PgnIO::tournament().setOptions(pgnOptions);
}

void handleEngineOptions() {
	EngineWorkerFactory::setSuppressInfoLines(CliSettings::Manager::get<bool>("rapid"));
    std::string enginesFile = CliSettings::Manager::get<std::string>("enginesfile");
    if (!enginesFile.empty()) {
        EngineWorkerFactory::getConfigManagerMutable().loadFromFile(enginesFile);
    }
    auto engineSettings = CliSettings::Manager::getGroupInstances("engine");
	auto eachSetting = CliSettings::Manager::getGroupInstance("each");
	CliSettings::ValueMap eachOptions;
	if (eachSetting) {
		eachOptions = eachSetting->getValues();
	}
    EngineConfig config;

    for (auto engine : engineSettings) {
        std::string cmd = engine.get<std::string>("cmd");
        std::string conf = engine.get<std::string>("conf");
        std::string name = engine.get<std::string>("name");

        CliSettings::ValueMap options = engine.getValues();
        options.insert(eachOptions.begin(), eachOptions.end());

        if (!cmd.empty()) {
            config = EngineConfig::createFromValueMap(options);
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
		}
		else if (!conf.empty()) {
			auto engineConfig = EngineWorkerFactory::getConfigManager().getConfig(conf);
			if (!engineConfig) {
				throw AppError::makeInvalidParameters("Engine configuration '" + conf + "' not found.");
			}
            config = *engineConfig;
			config.setCommandLineOptions(options, true);
            name = config.getName();
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
		}
		else {
            std::string engineName = name.empty() ? "" : " (for " + name + ")";
            throw AppError::makeInvalidParameters("No engine command or configuration provided"
                + engineName + ".Please specify either 'cmd' or 'conf'.");
		}

    }
    // Ensure that all active engines have different names
    EngineWorkerFactory::assignUniqueDisplayNames();
}

/**
 * Converts argc/argv into a vector of strings for easier manipulation.
 */
std::vector<std::string> argvToVector(int argc, char* argv[]) {
    std::vector<std::string> result;
    for (int i = 0; i < argc; ++i) {
        result.emplace_back(argv[i]);
    }
    return result;
}

AppReturnCode run(const std::vector<std::string>& args) {
    AppReturnCode returnCode = AppReturnCode::NoError;
    auto extendedArgs = CliSettings::Manager::mergeWithSettingsFile(args);
    CliSettings::Manager::parseCommandLine(extendedArgs);
    InputHandler::inputLoop(extendedArgs.size() == 1 || CliSettings::Manager::get<bool>("interactive"));

    handleGlobalOptions(returnCode);
    handlePgnOptions();
    handleEngineOptions();

    if (auto test = CliSettings::Manager::getGroupInstance("test")) {
        returnCode = runTest(*test, returnCode);
    }

    auto epdList = CliSettings::Manager::getGroupInstances("epd");
    if (!epdList.empty()) {
        returnCode = runEpd(epdList, returnCode);
    }

    auto tournament = CliSettings::Manager::getGroupInstance("tournament");
    if (tournament) {
        returnCode = runTournament(returnCode);
    }

    auto sprt = CliSettings::Manager::getGroupInstance("sprt");
    if (sprt) {
        returnCode = runSprt(returnCode);
    }
    return returnCode;
}

int main(int argc, char** argv) {
    bool isEngineTest = false;
    Timer timer;
    timer.start();
    

    AppReturnCode returnCode = AppReturnCode::NoError;
    try {
        Logger::testLogger().setTraceLevel(TraceLevel::command);
        Logger::testLogger().log("Qapla Engine Tester - Prerelease 0.4.0 (c) by Volker Boehm\n");

        CliSettings::Manager::registerSetting("interactive", "Enables interactive mode", false, false,  CliSettings::ValueType::Bool);
        CliSettings::Manager::registerSetting("concurrency", "Maximal number of in parallel running engines", true, 10,
            CliSettings::ValueType::Int);
        CliSettings::Manager::registerSetting("rapid", "Ignores engine info output. Speeds up games with <10s total compute time",
            false, false, CliSettings::ValueType::Bool);
        CliSettings::Manager::registerSetting("enginesfile", "Path to an ini file with engine configurations", false, "",
			CliSettings::ValueType::PathExists);
		CliSettings::Manager::registerSetting("enginelog", "Enable engine logging", false, false,
			CliSettings::ValueType::Bool);
        CliSettings::Manager::registerSetting("logpath", "Path to the logging directory", false, std::string(""), 
            CliSettings::ValueType::PathExists);
		CliSettings::Manager::registerSetting("settingsfile", "Path to a settings file in INI-style format", false, std::string(""),
			CliSettings::ValueType::PathExists);


        CliSettings::Manager::registerGroup("engine", "Defines an engine configuration", false, {
            { "conf",      { "Name of an engine from the configuration file", false, "", CliSettings::ValueType::String } },
            { "name",      { "Name of the engine", false, "", CliSettings::ValueType::String } },
            { "cmd",       { "Path to executable", false, "", CliSettings::ValueType::PathExists } },
            { "dir",       { "Working directory", false, std::nullopt, CliSettings::ValueType::PathExists}},
            { "proto",     { "Protocol (uci/xboard)", false, std::nullopt, CliSettings::ValueType::String } },
            { "tc",        { "Time control in format moves/time+inc or 'inf'", false, std::nullopt, CliSettings::ValueType::String } },
            { "ponder",    { "Enable pondering, if the engine supports it", false, std::nullopt, CliSettings::ValueType::Bool } },
            { "gauntlet",  { "Set if engine is part of the gauntlet group.", false, false, CliSettings::ValueType::Bool } },
			{ "trace",     { "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work", 
                false,std::nullopt, CliSettings::ValueType::String}},
            { "option.[name]",  { "UCI engine option", false, "", CliSettings::ValueType::String } }
            });
        CliSettings::Manager::registerGroup("each", "Defines configuration options for all engines", false, {
            { "dir",       { "Working directory", false, ".", CliSettings::ValueType::PathExists } },
            { "proto",     { "Protocol (uci/xboard)", false, "uci", CliSettings::ValueType::String } },
            { "tc",        { "Time control in format moves/time+inc or 'inf'", false, "", CliSettings::ValueType::String } },
            { "ponder",    { "Enable pondering, if the engine supports it", false, false, CliSettings::ValueType::Bool}},
            { "trace",     { "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work",
                false, "command", CliSettings::ValueType::String}},
            { "option.[name]",  { "UCI engine option", false, "", CliSettings::ValueType::String } }
            });

        CliSettings::Manager::registerGroup("epd", "Configuration to run an epd testset against engines", false, {
            { "file",      { "Path and file name to the epd file", true, "", CliSettings::ValueType::PathExists } },
            { "maxtime",   { "Maximum allowed time in seconds per move during EPD analysis.", false, 20, CliSettings::ValueType::Int } },
            { "mintime",   { "Minimum required time for an early stop, when a correct move is found", false, 2, CliSettings::ValueType::Int } },
            { "seenplies", { "Amount of plies one of the expected moves must be shown to stop early (-1 = off)", false, -1, CliSettings::ValueType::Int } },
            { "minsuccess", { "Minimum percentage of best moves that must be found", false, 0, CliSettings::ValueType::Int} }
            });

        CliSettings::Manager::registerGroup("sprt", "Sequential Probability Ratio Test configuration", true, {
            { "resultfile", { "File to save tournament outcome", false, "", CliSettings::ValueType::PathParentExists } },
            { "elolower",  { "Lower ELO bound for H1 (Engine 1 is considered stronger if at least eloLower Elo ahead)", false, 0, CliSettings::ValueType::Int } },
            { "eloupper",  { "Upper ELO bound for H0 (Test may stop early if Engine 1 is not stronger by at least eloUpper Elo)", false, 10, CliSettings::ValueType::Int } },
            { "alpha", { "Type I error threshold", false, 0.05f, CliSettings::ValueType::Float } },
            { "beta",  { "Type II error threshold", false, 0.05f, CliSettings::ValueType::Float } },
            { "maxgames", { "Maximum number of games before forced stop (0 = unlimited)", false, 0, CliSettings::ValueType::Int } },
			{ "montecarlo", { "Run Monte Carlo test instead of SPRT", false, false, CliSettings::ValueType::Bool } }
            });

        CliSettings::Manager::registerGroup("openings", "Defines how start positions are selected", true, {
            { "file",  { "Path to file with opening positions", true, "", CliSettings::ValueType::PathExists } },
            { "format", { "Format of the file: epd, raw, pgn", false, "epd", CliSettings::ValueType::String } },
            { "order", { "Order of position selection: random, sequential", false, "sequential", CliSettings::ValueType::String } },
            { "srand", { "Seed for random opening selection", false, 5489, CliSettings::ValueType::Int } },
            { "plies", { "Max number of plies per opening (all = unlimited)", false, "all", CliSettings::ValueType::String}},
            { "start", { "Index of first opening (1-based)", false, 1, CliSettings::ValueType::Int } },
            { "policy", { "Opening switch policy: default, encounter, round", false, "default", CliSettings::ValueType::String } }
            });

        CliSettings::Manager::registerGroup("test", "Test the engine", true, {
            { "underrun",   { "Check for movetime underruns", false, false, CliSettings::ValueType::Bool } },
            { "timeusage",  { "Check time usage in test games", false, false, CliSettings::ValueType::Bool } },
            { "numgames",   { "Number of test games to run", false, 20, CliSettings::ValueType::Int } },
            { "noponder",   { "Skip pondering test", false, false, CliSettings::ValueType::Bool } },
            { "noepd",      { "Skip EPD bestmove test", false, false, CliSettings::ValueType::Bool } },
            { "nomemory",   { "Skip hash table memory usage test", false, false, CliSettings::ValueType::Bool } },
            { "nooption",   { "Skip option crash tests", false, false, CliSettings::ValueType::Bool } },
            { "nostop",     { "Skip immediate stop response test", false, false, CliSettings::ValueType::Bool } },
            { "nowait",     { "Skip check that infinite search never returns", false, false, CliSettings::ValueType::Bool } }
            });

        CliSettings::Manager::registerGroup("pgnoutput", "PGN output settings", true, {
            { "file", { "Path to the output PGN file", true, "", CliSettings::ValueType::String } },
            { "append", { "Append to existing file instead of overwriting it", false, false, CliSettings::ValueType::Bool } },
            { "fi", { "Only save finished games", false, true, CliSettings::ValueType::Bool } },
            { "min", { "Only save minimal tag information in the PGN output", false, false, CliSettings::ValueType::Bool } },
            //{ "aftermove", { "Save after every move", false, false, CliSettings::ValueType::Bool } },
            { "clock", { "Include clock information in the PGN output", false, true, CliSettings::ValueType::Bool } },
            { "eval", { "Include evaluation values in the PGN output", false, true, CliSettings::ValueType::Bool } },
            { "depth", { "Include search depth in the PGN output", false, true, CliSettings::ValueType::Bool } },
            { "pv", { "Include principal variation in the PGN output", false, false, CliSettings::ValueType::Bool } }
            });

        CliSettings::Manager::registerGroup("tournament", "Tournament setup and general parameters", true, {
            { "type", { "Tournament type: gauntlet/round-robin", true, "gauntlet", CliSettings::ValueType::String } },
            { "resultfile", { "File to save tournament outcome", false, "", CliSettings::ValueType::PathParentExists } },
            { "append", { "Append to result file instead of overwriting it", false, false, CliSettings::ValueType::Bool } },
            { "event", { "Optional event name for PGN or logging", false, "", CliSettings::ValueType::String } },
            { "games", { "Number of games per pairing (total games = games * rounds)", false, 2, CliSettings::ValueType::Int } },
            { "rounds", { "Repeat all pairings this many times", false, 1, CliSettings::ValueType::Int } },
            { "repeat", { "Number of consecutive games using same opening (e.g. 2 with swapping colors)", false, 2, CliSettings::ValueType::Int } },
            { "noswap", { "Disable automatic color swap after each game", false, false, CliSettings::ValueType::Bool } },
            { "ratinginterval", { "Interval (in games) for printing rating table", false, 100, CliSettings::ValueType::Int } },
            { "averageelo", { "Set average Elo level for scaling rating output", false, 2600, CliSettings::ValueType::Int } },
            { "outcomeinterval", { "Interval (in games) for printing outcome table", false, 0, CliSettings::ValueType::Int } }
            });

		auto args = argvToVector(argc, argv); 
        returnCode = run(args);
			
    }
    catch (const AppError& ex) {
		Logger::testLogger().log("Application error: " + std::string(ex.what()), TraceLevel::error);
        returnCode = ex.getReturnCode();
    }
	catch (const std::exception& e) {
		Logger::testLogger().log(std::string(e.what()), TraceLevel::error);
        returnCode = AppReturnCode::GeneralError;
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception, program terminated.", TraceLevel::error);
		returnCode = AppReturnCode::GeneralError;
	}
	timer.printElapsed("Total runtime: ");
	
    // Unregisters the input handler callback before destruction of the input handler
	GameManagerPool::resetInstance();
    return static_cast<int>(returnCode);
}

