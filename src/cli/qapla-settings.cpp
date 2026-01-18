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

#include "qapla-settings.h"

#include "cli-settings-manager.h"
#include "../opening/pgn-io.h"
#include "../base-elements/app-error.h"
#include "../base-elements/logger.h"
#include "../opening/openings.h"
#include "../tournament/tournament.h"
#include "../sprt/sprt-manager.h"
#include "../sprt/sprt-tournament-file.h"
#include "../sprt/sprt-config-file.h"
#include "../config-file/opening-config.h"
#include "../config-file/pgn-config.h"
#include "../config-file/adjudication-config.h"
#include "../config-file/engine-config-file.h"
#include "../config-file/engine-global-config.h"
#include "../epd/epd-manager.h"
#include "../engine-handling/engine-worker-factory.h"
#include "../spsa/spsa-optimizer.h"

#include <format>

namespace QaplaTester::CliSettings {

QaplaSettings& QaplaSettings::instance() {
    static QaplaSettings instance;
    return instance;
}

void QaplaSettings::applyArguments(const std::vector<std::string>& args) {
    m_arguments = Manager::mergeWithSettingsFile(args);
    Manager::parseCommandLine(m_arguments);

    // Read options after all settings are registered and read.
    readLoggerConfig();
    readEngineOptions();
    readEngineGlobalConfig();
    readPgnOptions();
    readDrawAdjudicationConfig();
    readResignAdjudicationConfig();
    readOpenings();
    readTournamentConfig();
    readSprtConfig();
    readEpdConfig();
    readSPSAConfig();
}

const std::vector<std::string>& QaplaSettings::getArguments() const {
    return m_arguments;
}

const LoggerConfig* QaplaSettings::getLoggerConfig() const {
    return m_loggerConfig.get();
}

void QaplaSettings::applyLoggerConfig(const std::string& reportLogBaseName) const {
    if (m_loggerConfig) {
        LoggerConfig config = *m_loggerConfig;
        config.reportLogBaseName = reportLogBaseName;
        setLoggerConfig(config);
    } else {
        setLoggerConfig({
            .logPath = "./log",
            .reportLogBaseName = reportLogBaseName,
            .engineLogBaseName = "engine",
            .engineLogStrategy = LogFileStrategy::global
        });
    }
}

std::vector<std::string> QaplaSettings::argvToVector(int argc, char* argv[]) {
    std::vector<std::string> result;
    for (int i = 0; i < argc; ++i) {
        result.emplace_back(argv[i]);
    }
    return result;
}

void QaplaSettings::init() {
    // Global settings
    Manager::registerSetting("interactive", "Enables interactive mode", false, false, ValueType::Bool);
    Manager::registerSetting("concurrency", "Maximal number of in parallel running engines", true, 10,
        ValueType::UInt);
    Manager::registerSetting("rapid", "Ignores engine info output. Speeds up games with <10s total compute time",
        false, false, ValueType::Bool);
    Manager::registerSetting("enginesfile", "Path to an ini file with engine configurations", false, "",
        ValueType::PathExists);
    Manager::registerSetting("settingsfile", "Path to a settings file in INI-style format", false, std::string(""),
        ValueType::PathExists);

    // Engine group
    Manager::registerGroup("engine", "Defines an engine configuration", false, {
        { "conf",      { "Name of an engine from the configuration file", false, "", ValueType::String } },
        { "name",      { "Name of the engine", false, "", ValueType::String } },
        { "cmd",       { "Path to executable", false, "", ValueType::PathExists } },
        { "dir",       { "Working directory", false, std::nullopt, ValueType::PathExists}},
        { "proto",     { "Protocol (uci/xboard)", false, std::nullopt, ValueType::String } },
        { "tc",        { "Time control in format moves/time+inc or 'inf'", false, std::nullopt, ValueType::String } },
        { "ponder",    { "Enable pondering, if the engine supports it", false, std::nullopt, ValueType::Bool } },
        { "gauntlet",  { "Set if engine is part of the gauntlet group.", false, false, ValueType::Bool } },
        { "trace",     { "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work", 
            false, std::nullopt, ValueType::String}},
        { "restart", { "Engine restart mode: auto (engine decides), on (always), or off (never)",
            false, std::nullopt, ValueType::String }},
        { "option.[name]",  { "UCI engine option", false, "", ValueType::String } }
    });

    // Logging group
    Manager::registerGroup("logging", "Logger configuration", true, {
        { "engine", { 
            .description = "If true, engine logging is enabled", 
            .isRequired = false, 
            .defaultValue = true, 
            .type = ValueType::Bool }},
        { "path",   { 
            .description = "Path to the logging directory", 
            .isRequired = false,
            .defaultValue = std::string(""), 
            .type = ValueType::String }},
        { "mode",   { 
            .description = "Engine log file strategy: one (single file for all engines), each (separate file per engine)",
            .isRequired = false, 
            .defaultValue = std::string("one"), 
            .type = ValueType::String }},
    });

    // Each group
    Manager::registerGroup("each", "Defines configuration options for all engines", false, {
        { "dir",       { "Working directory", false, ".", ValueType::PathExists } },
        { "proto",     { "Protocol (uci/xboard)", false, "uci", ValueType::String } },
        { "tc",        { "Time control in format moves/time+inc or 'inf'", false, "", ValueType::String } },
        { "ponder",    { "Enable pondering, if the engine supports it", false, false, ValueType::Bool}},
        { "trace",     { "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work",
            false, "command", ValueType::String}},
        { "restart", { "Engine restart mode: auto (engine decides), on (always), or off (never)", 
            false, "auto", ValueType::String }},
        { "option.[name]",  { "UCI engine option", false, "", ValueType::String } }
    });

    // EPD group
    Manager::registerGroup("epd", "Configuration to run an epd testset against engines", false, {
        { "file",      { "Path and file name to the epd file", true, "", ValueType::PathExists } },
        { "maxtime",   { "Maximum allowed time in seconds per move during EPD analysis.", false, 20, ValueType::UInt } },
        { "mintime",   { "Minimum required time for an early stop, when a correct move is found", false, 2, ValueType::UInt } },
        { "seenplies", { "Amount of plies one of the expected moves must be shown to stop early (0 = off)", false, 0, ValueType::UInt } },
        { "minsuccess", { "Minimum percentage of best moves that must be found", false, 0, ValueType::UInt} }
    });

    // SPRT group
    Manager::registerGroup("sprt", "Sequential Probability Ratio Test configuration", true, {
        { "file", { .description = "File to load/save tournament outcome", 
                    .isRequired = false, 
                    .defaultValue = "", 
                    .type = ValueType::PathExists,
                    .exclusive = true } },
        { "saveinterval", { .description = "Interval in games to save tournament state", 
                            .isRequired = false, .defaultValue = 100, .type = ValueType::UInt } },
        { "elolower",  { "Lower ELO bound for H1 (Engine 1 is considered stronger if at least eloLower Elo ahead)", false, 0, ValueType::Int } },
        { "eloupper",  { "Upper ELO bound for H0 (Test may stop early if Engine 1 is not stronger by at least eloUpper Elo)", false, 10, ValueType::Int } },
        { "alpha", { "Type I error threshold", false, 0.05f, ValueType::Float } },
        { "beta",  { "Type II error threshold", false, 0.05f, ValueType::Float } },
        { "maxgames", { "Maximum number of games before forced stop (0 = unlimited)", false, 0, ValueType::UInt } },
        { "model", { "Model used for SPRT calculations normalized, logistic, bayesian", false, "normalized", ValueType::String } },
        { "pentanomial", { "Use pentanomial model for SPRT calculations", false, true, ValueType::Bool } },
        { "montecarlo", { "Run Monte Carlo test instead of SPRT", false, false, ValueType::Bool } }
    });

    // Openings group
    Manager::registerGroup("openings", "Defines how start positions are selected", true, {
        { "file",  { "Path to file with opening positions", true, "", ValueType::PathExists } },
        { "order", { "Order of position selection: random, sequential", false, "sequential", ValueType::String } },
        { "srand", { "Seed for random opening selection", false, 5489, ValueType::UInt } },
        { "plies", { "Max number of plies per opening (all = unlimited)", false, "all", ValueType::String}},
        { "start", { "Index of first opening (1-based)", false, 1, ValueType::UInt } },
        { "policy", { "Opening switch policy: default, encounter, round", false, "default", ValueType::String } }
    });

    // Test group
    Manager::registerGroup("test", "Test the engine", true, {
        { "underrun",   { "Check for movetime underruns", false, false, ValueType::Bool } },
        { "timeusage",  { "Check time usage in test games", false, false, ValueType::Bool } },
        { "numgames",   { "Number of test games to run", false, 20, ValueType::UInt } },
        { "noponder",   { "Skip pondering test", false, false, ValueType::Bool } },
        { "noepd",      { "Skip EPD bestmove test", false, false, ValueType::Bool } },
        { "nomemory",   { "Skip hash table memory usage test", false, false, ValueType::Bool } },
        { "nooption",   { "Skip option crash tests", false, false, ValueType::Bool } },
        { "nostop",     { "Skip immediate stop response test", false, false, ValueType::Bool } },
        { "nowait",     { "Skip check that infinite search never returns", false, false, ValueType::Bool } }
    });

    // PGN output group
    Manager::registerGroup("pgnoutput", "PGN output settings", true, {
        { "file", { "Path to the output PGN file", true, "", ValueType::String } },
        { "append", { "Append to existing file instead of overwriting it", false, false, ValueType::Bool } },
        { "fi", { "Only save finished games", false, true, ValueType::Bool } },
        { "min", { "Only save minimal tag information in the PGN output", false, false, ValueType::Bool } },
        { "clock", { "Include clock information in the PGN output", false, true, ValueType::Bool } },
        { "eval", { "Include evaluation values in the PGN output", false, true, ValueType::Bool } },
        { "depth", { "Include search depth in the PGN output", false, true, ValueType::Bool } },
        { "pv", { "Include principal variation in the PGN output", false, false, ValueType::Bool } }
    });

    // Tournament group
    Manager::registerGroup("tournament", "Tournament setup and general parameters", true, {
        { "type", { "Tournament type: gauntlet/round-robin", true, "gauntlet", ValueType::String } },
        { "file", { "File to save tournament state", false, "", ValueType::PathParentExists } },
        { "saveinterval", { "Interval in games to save tournament state", false, 10, ValueType::UInt } },
        { "append", { "Append to result file instead of overwriting it", false, false, ValueType::Bool } },
        { "event", { "Optional event name for PGN or logging", false, "", ValueType::String } },
        { "games", { "Number of games per pairing (total games = games * rounds)", false, 2, ValueType::UInt } },
        { "rounds", { "Repeat all pairings this many times", false, 1, ValueType::UInt } },
        { "repeat", { "Number of consecutive games using same opening (e.g. 2 with swapping colors)", false, 2, ValueType::UInt } },
        { "noswap", { "Disable automatic color swap after each game", false, false, ValueType::Bool } },
        { "ratinginterval", { "Interval (in games) for printing rating table", false, 100, ValueType::UInt } },
        { "averageelo", { "Set average Elo level for scaling rating output", false, 2600, ValueType::Int } },
        { "outcomeinterval", { "Interval (in games) for printing outcome table", false, 0, ValueType::UInt } }
    });

    // Draw adjudication group
    Manager::registerGroup("draw", "Draw adjudication settings", true, {
        { "movenumber", { "Minimum number of full moves before draw adjudication can occur", true, 0, ValueType::UInt } },
        { "movecount",  { "Required number of consecutive moves with evaluation in range", true, 0, ValueType::UInt } },
        { "score",      { "Centipawn score range (+/-) around zero for draw adjudication", true, 0, ValueType::Int } },
        { "test",       { "If true, only reports what would be adjudicated without taking action", false, false, ValueType::Bool } }
    });

    // Resign adjudication group
    Manager::registerGroup("resign", "Resign adjudication settings", true, {
        { "movecount", { .description = "Required number of consecutive moves with score below threshold for resignation", 
                        .isRequired = true, 
                        .defaultValue = 0, 
                        .type = ValueType::UInt } },
        { "score",     { .description = "Centipawn score below zero that triggers resignation", 
                        .isRequired = true, 
                        .defaultValue = 0, 
                        .type = ValueType::Int } },
        { "twosided",  { .description = "If true, both sides must meet respective score conditions", 
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "test",      { .description = "If true, only reports what would be adjudicated without taking action", 
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } }
    });

    // SPSA optimization group
    Manager::registerGroup("spsa", "SPSA parameter optimization configuration", true, {
        { "activepairs",   { .description = "Maximum number of concurrent unfinished tournament pairs", 
                            .isRequired = false, 
                            .defaultValue = 32, 
                            .type = ValueType::UInt } },
        { "learningrate",  { .description = "Global learning rate for parameter updates (r in SPSA algorithm)", 
                            .isRequired = false, 
                            .defaultValue = 0.002F, 
                            .type = ValueType::Float } },
        { "gamesperpair",  { .description = "Number of games per parameter perturbation pair", 
                            .isRequired = false, 
                            .defaultValue = 8, 
                            .type = ValueType::UInt } },
        { "iterations",    { .description = "Maximum number of optimization iterations", 
                            .isRequired = false, 
                            .defaultValue = 1000, 
                            .type = ValueType::UInt } },
        { "seed",          { .description = "Random seed for opening selection", 
                            .isRequired = false, 
                            .defaultValue = 0, 
                            .type = ValueType::UInt } },
        { "noswap",        { .description = "Disable automatic color swap between games", 
                            .isRequired = false, 
                            .defaultValue = false, 
                            .type = ValueType::Bool } }
    });

    // SPSA parameter value group
    Manager::registerGroup("spsavalue", "Defines a single parameter to optimize with SPSA", false, {
        { "name",      { .description = "UCI parameter name to optimize", 
                        .isRequired = true, 
                        .defaultValue = "", 
                        .type = ValueType::String } },
        { "default",   { .description = "Starting value for the parameter", 
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } },
        { "min",       { .description = "Minimum allowed value for the parameter", 
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } },
        { "max",       { .description = "Maximum allowed value for the parameter", 
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } },
        { "step",      { .description = "Perturbation step size (c_i in SPSA algorithm)", 
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } }
    });
}

void QaplaSettings::readLoggerConfig() {
    auto loggingSetting = Manager::getGroupInstance("logging");
    
    std::string logPath = "./log";
    std::string logModeStr = "one";
    
    if (loggingSetting) {
        logPath = loggingSetting->get<std::string>("path");
        logModeStr = loggingSetting->get<std::string>("mode");
    }
    
    LogFileStrategy logMode = (logModeStr == "each") ? LogFileStrategy::perEngine : LogFileStrategy::global;

    m_loggerConfig = std::make_unique<LoggerConfig>(LoggerConfig{
        .logPath = logPath,
        .reportLogBaseName = "report",
        .engineLogBaseName = "engine",
        .engineLogStrategy = logMode
    });
}

void QaplaSettings::readEngineOptions() {
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

    for (const auto& engine : engineSettings) {
        // Merge global options with engine-specific options (engine options take precedence)
        CliSettings::ValueMap mergedOptions = engine.getValues();
        mergedOptions.insert(eachOptions.begin(), eachOptions.end());

        // Check if cmd or conf is specified
        auto cmdIt = mergedOptions.find("cmd");
        auto confIt = mergedOptions.find("conf");
        auto nameIt = mergedOptions.find("name");
        
        std::string cmd = (cmdIt != mergedOptions.end() && std::holds_alternative<std::string>(cmdIt->second)) 
            ? std::get<std::string>(cmdIt->second) : "";
        std::string conf = (confIt != mergedOptions.end() && std::holds_alternative<std::string>(confIt->second))
            ? std::get<std::string>(confIt->second) : "";
        std::string name = (nameIt != mergedOptions.end() && std::holds_alternative<std::string>(nameIt->second))
            ? std::get<std::string>(nameIt->second) : "";

        EngineConfig config;

        if (!cmd.empty()) {
            config = EngineConfig::createFromValueMap(mergedOptions);
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
        }
        else if (!conf.empty()) {
            auto engineConfig = EngineWorkerFactory::getConfigManager().getConfig(conf);
            if (!engineConfig) {
                throw AppError::makeInvalidParameters("Engine configuration '" + conf + "' not found.");
            }
            config = *engineConfig;
            config.setCommandLineOptions(mergedOptions, true);
            EngineWorkerFactory::getActiveEnginesMutable().push_back(config);
        }
        else {
            std::string engineName = name.empty() ? "" : " (for " + name + ")";
            throw AppError::makeInvalidParameters("No engine command or configuration provided"
                + engineName + ". Please specify either 'cmd' or 'conf'.");
        }
    }
    // Ensure that all active engines have different names
    EngineWorkerFactory::assignUniqueDisplayNames();
}

void QaplaSettings::readEngineGlobalConfig() {
    auto eachSetting = Manager::getGroupInstance("each");
    if (!eachSetting) {
        m_engineGlobalConfig = nullptr;
        return;
    }

    const auto& each = *eachSetting;
    auto hashStr = each.get<std::string>("option.Hash");
    auto hashValue = QaplaHelpers::to_unsigned_int<uint32_t>(hashStr).value_or(0);

    m_engineGlobalConfig = std::make_unique<EngineGlobalConfig>(EngineGlobalConfig{
        .useGlobalHash = each.isKeyProvided("option.Hash"),
        .hashSizeMB = hashValue,
        .useGlobalPonder = each.isKeyProvided("ponder"),
        .ponder = each.get<bool>("ponder"),
        .useGlobalTrace = each.isKeyProvided("trace"),
        .traceLevel = each.get<std::string>("trace"),
        .useGlobalRestart = each.isKeyProvided("restart"),
        .restart = each.get<std::string>("restart"),
        .timeControl = each.get<std::string>("tc")
    });
}

void QaplaSettings::readPgnOptions() {
    auto pgnOptionInstance = Manager::getGroupInstance("pgnoutput");
    if (!pgnOptionInstance) {
        m_pgnOptions = nullptr;
        return;
    }

    const auto& pgn = *pgnOptionInstance;
    m_pgnOptions = std::make_unique<PgnSave::Options>(PgnSave::Options{
        .file = pgn.get<std::string>("file"),
        .append = pgn.get<bool>("append"),
        .onlyFinishedGames = pgn.get<bool>("fi"),
        .minimalTags = pgn.get<bool>("min"),
        .includeClock = pgn.get<bool>("clock"),
        .includeEval = pgn.get<bool>("eval"),
        .includePv = pgn.get<bool>("pv"),
        .includeDepth = pgn.get<bool>("depth")
    });
}

std::optional<PgnSave::Options> QaplaSettings::getPgnOptions() const {
    if (m_pgnOptions) {
        return *m_pgnOptions;
    }
    return std::nullopt;
}

void QaplaSettings::readDrawAdjudicationConfig() {
    auto draw = Manager::getGroupInstance("draw");
    if (!draw) {
        m_drawConfig = nullptr;
        return;
    }

    m_drawConfig = std::make_unique<AdjudicationManager::DrawAdjudicationConfig>(AdjudicationManager::DrawAdjudicationConfig{
        .minFullMoves = draw->get<unsigned int>("movenumber"),
        .requiredConsecutiveMoves = draw->get<unsigned int>("movecount"),
        .centipawnThreshold = draw->get<int>("score"),
        .testOnly = draw->get<bool>("test")
    });
}

std::optional<AdjudicationManager::DrawAdjudicationConfig> QaplaSettings::getDrawAdjudicationConfig() const {
    if (m_drawConfig) {
        return *m_drawConfig;
    }
    return std::nullopt;
}

void QaplaSettings::readResignAdjudicationConfig() {
    auto resign = Manager::getGroupInstance("resign");
    if (!resign) {
        m_resignConfig = nullptr;
        return;
    }

    m_resignConfig = std::make_unique<AdjudicationManager::ResignAdjudicationConfig>(AdjudicationManager::ResignAdjudicationConfig{
        .requiredConsecutiveMoves = resign->get<unsigned int>("movecount"),
        .centipawnThreshold = resign->get<int>("score"),
        .testOnly = resign->get<bool>("test")
    });
}

std::optional<AdjudicationManager::ResignAdjudicationConfig> QaplaSettings::getResignAdjudicationConfig() const {
    if (m_resignConfig) {
        return *m_resignConfig;
    }
    return std::nullopt;
}

void QaplaSettings::readOpenings() {
    auto opening = Manager::getGroupInstance("openings");
    if (!opening) {
        m_openings = nullptr;
        return;
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

    auto openings = std::make_unique<Openings>(Openings{
        .file = opening->get<std::string>("file"),
        .order = opening->get<std::string>("order"),
        .plies = plies,
        .start = opening->get<unsigned int>("start"),
        .seed = opening->get<unsigned int>("srand"),
        .policy = opening->get<std::string>("policy")
    });

    if (openings->start < 1) {
        throw AppError::makeInvalidParameters("Openings: Start index must be at least 1, but got " +
            std::to_string(openings->start));
    }
    openings->start--; // 0-based

    if (openings->order != "sequential" && openings->order != "random") {
        throw AppError::makeInvalidParameters("Unsupported openings order: " + openings->order);
    }
    if (openings->policy != "default" && openings->policy != "encounter" && openings->policy != "round") {
        throw AppError::makeInvalidParameters("Unsupported openings policy: " + openings->policy);
    }

    m_openings = std::move(openings);
}

std::optional<Openings> QaplaSettings::getOpenings() const {
    if (!m_openings) {
        return std::nullopt;
    }
    return *m_openings;
}

void QaplaSettings::readTournamentConfig() {
    auto tournamentGroup = Manager::getGroupInstance("tournament");
    if (!tournamentGroup) {
        m_tournamentConfig = nullptr;
        return;
    }

    // Tournament needs openings
    if (!m_openings) {
        m_tournamentConfig = nullptr;
        return;
    }

    m_tournamentConfig = std::make_unique<TournamentConfig>(TournamentConfig{
        .event = tournamentGroup->get<std::string>("event"),
        .type = tournamentGroup->get<std::string>("type"),
        .tournamentFilename = tournamentGroup->get<std::string>("resultfile"),
        .saveInterval = tournamentGroup->get<unsigned int>("saveinterval"),
        .games = tournamentGroup->get<unsigned int>("games"),
        .rounds = tournamentGroup->get<unsigned int>("rounds"),
        .repeat = tournamentGroup->get<unsigned int>("repeat"),
        .ratingInterval = tournamentGroup->get<unsigned int>("ratinginterval"),
        .outcomeInterval = tournamentGroup->get<unsigned int>("outcomeinterval"),
        .averageElo = tournamentGroup->get<int>("averageelo"),
        .noSwap = tournamentGroup->get<bool>("noswap"),
        .openings = *m_openings
    });
}

std::optional<TournamentConfig> QaplaSettings::getTournamentConfig() const {
    if (!m_tournamentConfig) {
        return std::nullopt;
    }
    return *m_tournamentConfig;
}

void QaplaSettings::readSprtConfig() {
    auto sprt = Manager::getGroupInstance("sprt");
    if (!sprt) {
        m_sprtConfig = nullptr;
        return;
    }

    auto sprtFile = sprt->get<std::string>("file");
    if (!sprtFile.empty()) {
        QaplaHelpers::ConfigData configData;
        SprtTournamentFile::load(sprtFile, configData, "sprt-tournament");
        setFromConfigData(configData, "sprt-tournament");
        return;
    }

    // SPRT needs openings (unless montecarlo)
    auto isMontecarlo = sprt->get<bool>("montecarlo");
    if (!m_openings && !isMontecarlo && sprtFile.empty()) {
        m_sprtConfig = nullptr;
        return;
    }

    m_sprtConfig = std::make_unique<SprtConfig>(SprtConfig{
        .eloUpper = static_cast<float>(sprt->get<int>("eloUpper")),
        .eloLower = static_cast<float>(sprt->get<int>("eloLower")),
        .alpha = sprt->get<double>("alpha"),
        .beta = sprt->get<double>("beta"),
        .maxGames = sprt->get<unsigned int>("maxgames"),
        .model = sprt->get<std::string>("model"),
        .pentanomial = sprt->get<bool>("pentanomial"),
        .openings = m_openings ? *m_openings : Openings{}
    });

}

std::optional<SprtConfig> QaplaSettings::getSprtConfig() const {
    if (!m_sprtConfig) {
        return std::nullopt;
    }
    return *m_sprtConfig;
}

void QaplaSettings::readEpdConfig() {
    auto epdGroup = Manager::getGroupInstance("epd");
    if (!epdGroup) {
        m_epdConfig = nullptr;
        return;
    }

    m_epdConfig = std::make_unique<EpdConfig>(EpdConfig{
        .file = epdGroup->get<std::string>("file"),
        .maxTime = epdGroup->get<unsigned int>("maxtime"),
        .minTime = epdGroup->get<unsigned int>("mintime"),
        .seenPlies = epdGroup->get<unsigned int>("seenplies"),
        .minSuccess = epdGroup->get<unsigned int>("minsuccess")
    });
}

std::optional<EpdConfig> QaplaSettings::getEpdConfig() const {
    if (!m_epdConfig) {
        return std::nullopt;
    }
    return *m_epdConfig;
}

void QaplaSettings::readSPSAConfig() {
    auto spsaGroup = Manager::getGroupInstance("spsa");
    if (!spsaGroup) {
        m_spsaConfig = nullptr;
        return;
    }

    // SPSA needs openings
    if (!m_openings) {
        m_spsaConfig = nullptr;
        return;
    }

    m_spsaConfig = std::make_unique<SPSAConfig>();
    
    // Read basic SPSA configuration
    
    m_spsaConfig->maxActivePairs = spsaGroup->get<unsigned int>("activepairs");
    m_spsaConfig->learningRate = spsaGroup->get<double>("learningrate");
    m_spsaConfig->gamesPerPair = spsaGroup->get<unsigned int>("gamesperpair");
    m_spsaConfig->iterations = spsaGroup->get<unsigned int>("iterations");
    m_spsaConfig->openingsSeed = spsaGroup->get<unsigned int>("seed");
    m_spsaConfig->swapColors = !spsaGroup->get<bool>("noswap");
    m_spsaConfig->openingsFile = m_openings->file;
    
    // Read all spsavalue groups to build parameter list
    auto spsaValueGroups = Manager::getGroupInstances("spsavalue");
    for (const auto& valueGroup : spsaValueGroups) {
        SPSAParameterConfig param;
        param.name = valueGroup.get<std::string>("name");
        param.defaultValue = valueGroup.get<double>("default");
        param.minValue = valueGroup.get<double>("min");
        param.maxValue = valueGroup.get<double>("max");
        param.c = valueGroup.get<double>("step");
        
        m_spsaConfig->parameters.push_back(param);
    }

    // Validate that at least one parameter is defined
    if (m_spsaConfig->parameters.empty()) {
        m_spsaConfig = nullptr;
    }
}

std::optional<SPSAConfig> QaplaSettings::getSPSAConfig() const {
    if (!m_spsaConfig) {
        return std::nullopt;
    }
    return *m_spsaConfig;
}


void QaplaSettings::setFromConfigData(const QaplaHelpers::ConfigData& configData, const std::string& id) {
    // Apply SPRT configuration
    auto sprtConfig = SprtConfigFile::fromConfigData(configData, id);
    if (sprtConfig) {
        m_sprtConfig = std::make_unique<SprtConfig>(*sprtConfig);
    }

    // Apply Openings configuration
    auto openings = OpeningConfig::fromConfigData(configData, id);
    if (openings) {
        m_openings = std::make_unique<Openings>(*openings);
        if (m_sprtConfig) {
            m_sprtConfig->openings = *openings;
        }
    }

    // Apply PGN configuration
    auto pgnOptions = PgnConfig::fromConfigData(configData, id);
    if (pgnOptions) {
        m_pgnOptions = std::make_unique<PgnSave::Options>(*pgnOptions);
    }

    // Apply Draw Adjudication configuration
    auto drawConfig = AdjudicationConfig::fromDrawConfigData(configData, id);
    if (drawConfig) {
        m_drawConfig = std::make_unique<AdjudicationManager::DrawAdjudicationConfig>(*drawConfig);
    }

    // Apply Resign Adjudication configuration
    auto resignConfig = AdjudicationConfig::fromResignConfigData(configData, id);
    if (resignConfig) {
        m_resignConfig = std::make_unique<AdjudicationManager::ResignAdjudicationConfig>(*resignConfig);
    }

    // Load global engine configuration
    auto globalConfig = EngineGlobalConfigFile::fromConfigData(configData, id);
    if (globalConfig) {
        m_engineGlobalConfig = std::make_unique<EngineGlobalConfig>(*globalConfig);
    }
    
    // Load engine-specific configurations
    auto engineConfigs = EngineConfigFile::fromConfigData(configData, id);
    
    if (engineConfigs && !engineConfigs->empty()) {
        // Store ALL engine configurations (selected and non-selected)
        m_allEngineConfigurations = *engineConfigs;
        
        // Clear existing engines and add new ones
        EngineWorkerFactory::getActiveEnginesMutable().clear();
        
        for (auto& engineConfiguration : *engineConfigs) {
            // Only add engines that are selected for the tournament
            if (!engineConfiguration.selected) {
                continue;
            }
            
            // Apply global configuration to each engine if available
            if (globalConfig) {
                EngineGlobalConfigFile::applyGlobalConfig(engineConfiguration.config, *globalConfig);
            }
            
            EngineWorkerFactory::getActiveEnginesMutable().push_back(engineConfiguration.config);
        }
        
        EngineWorkerFactory::assignUniqueDisplayNames();
    }
}

const std::vector<EngineConfiguration>& QaplaSettings::getAllEngineConfigurations() const {
    return m_allEngineConfigurations;
}

QaplaHelpers::ConfigData QaplaSettings::getConfigData(const std::string& id) const {
    QaplaHelpers::ConfigData configData;

    // Convert Engine Global configuration back to sections
    if (m_engineGlobalConfig) {
        auto engineSections = EngineGlobalConfigFile::toEngineConfigSections(*m_engineGlobalConfig, id);
        configData.setSectionList("eachengine", id, engineSections);
        
        auto timeControlSections = EngineGlobalConfigFile::toTimeControlSections(*m_engineGlobalConfig, id);
        configData.setSectionList("timecontroloptions", id, timeControlSections);
    }

    if (m_openings) {
        auto sections = OpeningConfig::toSections(*m_openings, id);
        configData.setSectionList(OpeningConfig::getSectionName(), id, sections);
    }

    if (m_sprtConfig) {
        auto sections = SprtConfigFile::toSections(*m_sprtConfig, id);
        configData.setSectionList(SprtConfigFile::getSectionName(), id, sections);
    }

    if (m_pgnOptions) {
        auto sections = PgnConfig::toSections(*m_pgnOptions, id);
        configData.setSectionList(PgnConfig::getSectionName(), id, sections);
    }

    if (m_drawConfig) {
        auto sections = AdjudicationConfig::toDrawSections(*m_drawConfig, id);
        configData.setSectionList(AdjudicationConfig::getDrawSectionName(), id, sections);
    }

    if (m_resignConfig) {
        auto sections = AdjudicationConfig::toResignSections(*m_resignConfig, id);
        configData.setSectionList(AdjudicationConfig::getResignSectionName(), id, sections);
    }


    if (!m_allEngineConfigurations.empty()) {
        auto sections = EngineConfigFile::toSections(m_allEngineConfigurations, id);
        configData.setSectionList(EngineConfigFile::getSectionName(), id, sections);
    }

    return configData;
}

} // namespace QaplaTester::CliSettings


