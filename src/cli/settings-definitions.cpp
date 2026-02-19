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

#include "settings-definitions.h"

namespace QaplaTester::Settings {

void initSettings() {
    // Global settings
    Manager::instance().registerSetting({
        .name = "interactive", 
        .description = "Enables interactive mode", 
        .isRequired = false, 
        .defaultValue = false, 
        .type = ValueType::Bool
    });

    Manager::instance().registerSetting({
        .name = "mcp", 
        .description = "Enables Model Context Protocol (MCP) server mode", 
        .isRequired = false, 
        .defaultValue = false, 
        .type = ValueType::Bool
    });
    
    Manager::instance().registerSetting({
        .name = "concurrency", 
        .description = "Maximal number of in parallel running engines", 
        .longDescription = "Maximal number of in parallel running engines. A typical value is 'physical cores - 1'.",
        .isRequired = true, 
        .defaultValue = 10,
        .type = ValueType::UInt
    });
    
    Manager::instance().registerSetting({
        .name = "rapid", 
        .description = "Enables rapid mode (suppresses engine info lines)",
        .isRequired = false, 
        .defaultValue = false, 
        .type = ValueType::Bool
    });

    Manager::instance().registerSetting({
        .name = "enginesfile", 
        .description = "Path to an ini file with engine configurations", 
        .isRequired = false, 
        .defaultValue = "",
        .type = ValueType::PathExists
    });

    Manager::instance().registerSetting({
        .name = "settingsfile", 
        .description = "Path to a settings file in INI-style format", 
        .isRequired = false, 
        .defaultValue = std::string(""),
        .type = ValueType::PathExists
    });

    Manager::instance().registerGroup({
        .name = "engine", 
        .description = "Defines an engine configuration", 
        .unique = false, 
        .primaryKey = {"id", "name"},
        .keys = Settings::getEngineKeys()}
    );

    // Logging group
    Manager::instance().registerGroup({
        .name = "logging", 
        .description = "Logger configuration", 
        .unique = true, 
        .primaryKey = {"id"},
        .keys = Settings::getLoggingKeys()
    });

    // Each group
    Manager::instance().registerGroup({
        .name = "each", 
        .description = "Defines configuration options for all engines", 
        .unique = true, 
        .primaryKey = {"id"},
        .keys = Settings::getEachKeys()
    });

    // EPD group
    Manager::instance().registerGroup({
        .name = "epd", 
        .description = "Configuration to run an epd testset against engines", 
        .longDescription = R"(Runs an EPD (Extended Position Description) testset.
Each engine analyzes a set of positions and its performance is measured by how many 'best moves' it finds within the time limit.
Results are reported as a success rate and compared against a minimum threshold.)",
        .unique = true, 
        .keys = Settings::getEpdKeys()
    });

    // SPRT group
    Manager::instance().registerGroup({
        .name = "sprt", 
        .description = "Sequential Probability Ratio Test configuration", 
        .longDescription = R"(Runs SPRT (Sequential Probability Ratio Test).
Determines if Challenger is stronger than Baseline.
Roles:
- Challenger: Engine with 'gauntlet=true', or first engine if no gauntlet flag.
- Baseline: The other engine.
Hypotheses:
- H0 (Null): Challenger Elo <= Baseline + eloH0
- H1 (Alt):  Challenger Elo >= Baseline + eloH1
Configs:
- Improvement: eloH0=0, eloH1=5
- Regression: eloH0=-5, eloH1=0)",
        .unique = true, 
        .keys = Settings::getSprtKeys()
    });
    // Openings group
    Manager::instance().registerGroup({
        .name = "openings", 
        .description = "Defines how start positions are selected", 
        .unique = true, 
        .keys = Settings::getOpeningsKeys()
    });

    // Test group
    Manager::instance().registerGroup({
        .name = "test", 
        .description = "Test the engine", 
        .longDescription = "Runs an extended test to evaluate engine stability and performance.         "
            "The test involves granular steps designed to trigger potential issues,      "
            "including crashes with invalid parameters. Each test type can be disabled;  "
            "all tests are on by default. Disable specific tests if a fast result is     "
            "more important than comprehensive coverage.",
        .unique = true, 
        .keys = Settings::getTestKeys()
    });

    // PGN output group
    Manager::instance().registerGroup({
        .name = "pgnoutput", 
        .description = "PGN output settings", 
        .unique = true, 
        .primaryKey = {"id"},
        .keys = Settings::getPgnOutputKeys()
    });

    // Tournament group
    Manager::instance().registerGroup({
        .name = "tournament", 
        .description = "Tournament setup and general parameters", 
        .longDescription = R"(Runs a tournament between multiple engines.
Pairings are generated based on the tournament type (e.g., round-robin or gauntlet).
Engines play against each other with color swapping and opening variations.)",
        .unique = true, 
        .keys = Settings::getTournamentKeys()
    });

    // Draw adjudication group
    Manager::instance().registerGroup({
        .name = "draw", 
        .description = "Draw adjudication settings", 
        .longDescription = "Configures global draw adjudication rules. These settings apply to all tournament modes (SPRT, Tournament, SPSA) unless explicitly overridden.",
        .unique = true, 
        .primaryKey = {"id"},
        .keys = Settings::getDrawAdjudicationKeys()
    });

    // Resign adjudication group
    Manager::instance().registerGroup({
        .name = "resign", 
        .description = "Resign adjudication settings", 
        .longDescription = "Configures global resignation adjudication rules. These settings apply to all tournament modes (SPRT, Tournament, SPSA) unless explicitly overridden.",
        .unique = true, 
        .primaryKey = {"id"},
        .keys = Settings::getResignAdjudicationKeys()
    });

    // SPSA optimization group
    Manager::instance().registerGroup({
        .name = "spsa", 
        .description = "SPSA parameter optimization configuration", 
        .longDescription = R"(Optimizes engine parameters using the Simultaneous Perturbation Stochastic Approximation (SPSA) algorithm.
Parameters are perturbed in multiple iterations to find the optimal values that maximize playing strength.
The process requires two engines; the second engine will be automatically configured with the perturbed parameters.
Each iteration involves running a set of games with slightly different parameter values to estimate the gradient of the performance.
IMPORTANT: You MUST define ALL parameters you want to optimize using the 'spsavalue' group. 
Each 'spsavalue' must be fully defined with name, default, min, max, and step.)",
        .unique = true, 
        .keys = Settings::getSpsaKeys()
    });

    // SPSA parameter value group
    Manager::instance().registerGroup({
        .name = "spsavalue", 
        .description = "Defines a single parameter to optimize with SPSA", 
        .longDescription = R"(Defines a parameter for SPSA optimization.
All fields (name, default, min, max, step) are mandatory for the optimization process.
Multiple 'spsavalue' groups can be defined to optimize several parameters simultaneously.)",
        .unique = false, 
        .keys = Settings::getSpsaValueKeys()
    });

    // Round Group
    Manager::instance().registerGroup({
        .name = "round", 
        .description = "Information about played rounds", 
        .longDescription = "",
        .unique = false, 
        .keys = {},
        .ignore = true
    });
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getEngineKeys() {
    return {
        { "id",        { .description = "Identifier for the configuration", 
                        .isRequired = false, 
                        .defaultValue = "all", 
                        .type = ValueType::String,
                        .isHidden = true } },
        { "conf",      { .description = "Name of an engine from the configuration file", 
                        .longDescription = "Selects a pre-configured engine from the registry. You can see available engines via the 'manage_engines' tool.",
                        .isRequired = false, 
                        .defaultValue = "", 
                        .type = ValueType::String } },
        { "name",      { .description = "Name of the engine", 
                        .isRequired = false, 
                        .defaultValue = "", 
                        .type = ValueType::String } },
        { "originalName", { .description = "Original name of the engine before modification",
                        .isRequired = false,
                        .defaultValue = "",
                        .type = ValueType::String,
                        .isHidden = true } },
        { "selected",  { .description = "Whether this engine is selected for the tournament",
                        .isRequired = false,
                        .defaultValue = false,
                        .type = ValueType::Bool,
                        .isHidden = true } },
        { "author",    { .description = "Author of the engine",
                        .isRequired = false,
                        .defaultValue = "",
                        .type = ValueType::String,
                        .isHidden = true } },
        { "cmd",       { .description = "Path to executable", 
                        .longDescription = "Directly specify the path to the engine executable. Use this if the engine is not in the registry.",
                        .isRequired = false, 
                        .defaultValue = "", 
                        .type = ValueType::PathExists } },
        { "dir",       { .description = "Working directory", 
                        .isRequired = false, 
                        .defaultValue = std::nullopt, 
                        .type = ValueType::PathExists}},
        { "proto",     { .description = "Protocol (uci/xboard)", 
                        .isRequired = false, 
                        .defaultValue = std::nullopt, 
                        .type = ValueType::String } },
        { "tc",        { .description = "Time control in format moves/time+inc or 'inf'", 
                        .isRequired = false, 
                        .defaultValue = std::nullopt, 
                        .type = ValueType::String } },
        { "ponder",    { .description = "Enable pondering, if the engine supports it", 
                        .isRequired = false, 
                        .defaultValue = std::nullopt, 
                        .type = ValueType::Bool } },
        { "gauntlet",  { .description = "Set if engine is part of the gauntlet group.", 
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "trace",     { .description = "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work", 
                        .isRequired = false, 
                        .defaultValue = std::nullopt, 
                        .type = ValueType::String}},
        { "restart",   { 
            .description = "Engine restart mode: auto (engine decides), on (always), or off (never)",
            .isRequired = false, 
            .defaultValue = std::nullopt, 
            .type = ValueType::String }},
        { "option.[name]",  { 
            .description = "UCI engine option", 
            .longDescription = "",
            .isRequired = false, 
            .defaultValue = "", 
            .type = ValueType::String } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getLoggingKeys() {
    return {
        { "id",     { 
            .description = "Identifier for the configuration", 
            .isRequired = false, 
            .defaultValue = "default", 
            .type = ValueType::String,
            .isHidden = true }},
        { "engine", { 
            .description = "If true, engine logging is enabled", 
            .isRequired = false, 
            .defaultValue = true, 
            .type = ValueType::Bool }},
        { "path",   { 
            .description = "Path to the logging directory", 
            .isRequired = false,
            .defaultValue = std::string("."), 
            .type = ValueType::PathExists }},
        { "mode",   { 
            .description = "Engine log file strategy: one (single file for all engines), each (separate file per engine)",
            .isRequired = false, 
            .defaultValue = std::string("one"), 
            .type = ValueType::String }},
        { "trace",  { 
            .description = "CLI logging level: none, result, all",
            .isRequired = false, 
            .defaultValue = std::string("result"), 
            .type = ValueType::String }},
        { "mcp",    { 
            .description = "MCP logging level: none, result, all",
            .isRequired = false, 
            .defaultValue = std::string("result"), 
            .type = ValueType::String }},
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getEachKeys() {
    return {
        { "id",        { .description = "Identifier for the configuration", 
                        .isRequired = false, 
                        .defaultValue = "all", 
                        .type = ValueType::String,
                        .isHidden = true } },
        { "dir",       { .description = "Working directory", 
                        .isRequired = false, 
                        .defaultValue = ".", 
                        .type = ValueType::PathExists } },
        { "proto",     { .description = "Protocol (uci/xboard)", 
                        .isRequired = false, 
                        .defaultValue = "uci", 
                        .type = ValueType::String } },
        { "tc",        { .description = "Time control in format moves/time+inc or 'inf'", 
                        .isRequired = false, 
                        .defaultValue = "3+0.02", 
                        .type = ValueType::String } },
        { "ponder",    { .description = "Enable pondering, if the engine supports it", 
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool}},
        { "trace",     { .description = "Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work",
                        .isRequired = false, 
                        .defaultValue = "command", 
                        .type = ValueType::String}},
        { "restart",   { .description = "Engine restart mode: auto (engine decides), on (always), or off (never)", 
                        .isRequired = false, 
                        .defaultValue = "auto", 
                        .type = ValueType::String }},
        { "option.[name]",  { .description = "UCI engine option", 
                        .isRequired = false, 
                        .defaultValue = "", 
                        .type = ValueType::String } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getEpdKeys() {
    return {
        { "id",        { .description = "Identifier for the configuration", 
                        .isRequired = false, 
                        .defaultValue = "epd", 
                        .type = ValueType::String,
                        .isHidden = true } },
        { "file",      { .description = "Path and file name to the epd file", 
                        .isRequired = true, 
                        .defaultValue = "", 
                        .type = ValueType::PathExists } },
        { "maxtime",   { .description = "Maximum allowed time in seconds per move during EPD analysis.", 
                        .isRequired = false, 
                        .defaultValue = 20, 
                        .type = ValueType::UInt } },
        { "mintime",   { .description = "Minimum required time for an early stop, when a correct move is found", 
                        .isRequired = false, 
                        .defaultValue = 2, 
                        .type = ValueType::UInt } },
        { "seenplies", { .description = "Amount of plies one of the expected moves must be shown to stop early (0 = off)", 
                        .isRequired = false, 
                        .defaultValue = 0, 
                        .type = ValueType::UInt } },
        { "minsuccess", { .description = "Minimum percentage of best moves that must be found", 
                        .longDescription = "If the success rate is below this threshold, the program will exit with code 13 (MissedTarget).",
                        .isRequired = false, 
                        .defaultValue = 0, 
                        .type = ValueType::UInt} }
    };
}


QaplaHelpers::StableMap<std::string, ParameterDefinition> getSprtKeys() {
    return {
        { "id",   { .description = "Identifier for the configuration", 
                    .isRequired = false, 
                    .defaultValue = "sprt", 
                    .type = ValueType::String,
                    .isHidden = true } },
        { "file", { .description = "File to load/save tournament outcome", 
                    .isRequired = false, 
                    .defaultValue = "", 
                    .type = ValueType::ValidateOutputPath,
                    .exclusive = false } },
        { "saveintervalS", { .description = "Interval in seconds to save tournament state", 
                            .isRequired = false, 
                            .defaultValue = 10, 
                            .type = ValueType::UInt } },
        { "eloH0",  { .description = "H0: If accepted, not stronger by at least eloH0 elo", 
                        .longDescription = "The Elo parameter for the null hypothesis (H0). If the result supports this hypothesis, we conclude that Engine 1's advantage is at most 'eloH0' Elo.",
                        .isRequired = false, 
                        .defaultValue = 0.0, 
                        .type = ValueType::Float } },
        { "eloH1",  { .description = "H1: If accepted, stronger by at least eloH1 elo", 
                        .longDescription = "The Elo parameter for the alternative hypothesis (H1). If the result supports this hypothesis, we conclude that Engine 1's advantage is at least 'eloH1' Elo.",
                        .isRequired = false, 
                        .defaultValue = 5.0, 
                        .type = ValueType::Float } },
        { "alpha", { .description = "Type I error threshold", 
                    .isRequired = false, 
                    .defaultValue = 0.05F, 
                    .type = ValueType::Float } },
        { "beta",  { .description = "Type II error threshold", 
                    .isRequired = false, 
                    .defaultValue = 0.05F, 
                    .type = ValueType::Float } },
        { "maxgames", { .description = "Maximum number of games before forced stop", 
                        .longDescription = "Always set a limit of the maximum amount of games. Low limits are around 10000, a detailed analysis for small elo improvements (e.g. lower:0, uppder:3) would be best with limits around 50000 to 200000 games.",
                        .isRequired = true, 
                        .defaultValue = 10000, 
                        .type = ValueType::UInt } },
        { "model", { .description = "Model used for SPRT calculations normalized, logistic, bayesian", 
                    .isRequired = false, 
                    .defaultValue = "normalized", 
                    .type = ValueType::String } },
        { "pentanomial", { .description = "Use pentanomial model for SPRT calculations", 
                        .isRequired = false, 
                        .defaultValue = true, 
                        .type = ValueType::Bool } },
        { "montecarlo", { .description = "Run Monte Carlo test instead of SPRT", 
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getOpeningsKeys() {
    return {
        { "id",    { .description = "Identifier for the configuration", 
                    .isRequired = false, 
                    .defaultValue = "all", 
                    .type = ValueType::String,
                    .isHidden = true } },
        { "file",  { .description = "Path to file with opening positions", 
                    .isRequired = true, 
                    .defaultValue = "", 
                    .type = ValueType::PathExists } },
        { "order", { .description = "Order of position selection: random, sequential", 
                    .isRequired = false, 
                    .defaultValue = "sequential", 
                    .type = ValueType::String } },
        { "srand", { .description = "Seed for random opening selection", 
                    .isRequired = false, 
                    .defaultValue = 5489, 
                    .type = ValueType::UInt } },
        { "plies", { .description = "Max number of plies per opening (all = unlimited)", 
                    .isRequired = false, 
                    .defaultValue = "all", 
                    .type = ValueType::String}},
        { "start", { .description = "Index of first opening (1-based)", 
                    .isRequired = false, 
                    .defaultValue = 1, 
                    .type = ValueType::UInt } },
        { "policy", { .description = "Opening switch policy: default, encounter, round", 
                    .isRequired = false, 
                    .defaultValue = "default", 
                    .type = ValueType::String } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getTestKeys() {
    return {
        { "id",         { .description = "Identifier for the configuration", 
                        .isRequired = false, 
                        .defaultValue = "test", 
                        .type = ValueType::String,
                        .isHidden = true } },
        { "underrun",   { .description = "Check for movetime underruns", 
                        .longDescription = "Verifies that the engine does not stop searching too early before the allocated time is used up.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "timeusage",  { .description = "Check time usage in test games", 
                        .longDescription = "Measures and validates the engine's time management accuracy during actual games.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "numgames",   { .description = "Number of test games to run", 
                        .longDescription = "Specifies the number of games played by the engine against itself to test long-term stability (0 skips this test). Long running, many full engine vs. engine games.",
                        .isRequired = false, 
                        .defaultValue = 20, 
                        .type = ValueType::UInt } },
        { "noponder",   { .description = "Skip pondering test", 
                        .longDescription = "Disables tests for the 'ponder' UCI command, which checks if the engine correctly thinks during the opponent's time. (long running, full engine vs. engine game ponder on)",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "noepd",      { .description = "Skip EPD bestmove test", 
                        .longDescription = "Disables the EPD test, where the engine must find best moves in specific chess positions within a time limit.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "nomemory",   { .description = "Skip hash table memory usage test", 
                        .longDescription = "Disables the hash table test, which verifies if the engine correctly allocates and frees memory when the 'Hash' option is changed.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "nooption",   { .description = "Skip option crash tests", 
                        .longDescription = "Disables the parameter smoke test. WARNING: This test is very long-running and intentionally uses invalid or edge-case values to trigger crashes. Highly recommended to disable for quick checks.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "nostop",     { .description = "Skip immediate stop response test", 
                        .longDescription = "Disables testing of the 'stop' command to ensure the engine interrupts its search immediately.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "nowait",     { .description = "Skip check that infinite search never returns", 
                        .longDescription = "Disables the infinite mode test, ensuring the engine does not prematurely exit its search when no limits are set.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "noanalyze",  { .description = "Skip standard analysis test", 
                        .longDescription = "Disables the standard analysis test, verifying basic move generation and evaluation.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "nogolimits", { .description = "Skip test for go limits (depth, nodes, movetime)", 
                        .longDescription = "Disables tests for specific search limits like 'depth', 'nodes', and 'movetime'.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "nofens",     { .description = "Skip test for games from FEN", 
                        .longDescription = "Disables testing of move generation from varied FEN positions including En Passant and castling rights.",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } },
        { "nocompute",  { .description = "Skip test for single compute game", 
                        .longDescription = "Disables the single self-play game test used to verify end-to-end move flow. (long running, full engine vs. engine game)",
                        .isRequired = false, 
                        .defaultValue = false, 
                        .type = ValueType::Bool } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getPgnOutputKeys() {
    return {
        { "id",   { .description = "Identifier for the configuration", 
                    .isRequired = false, 
                    .defaultValue = "all", 
                    .type = ValueType::String,
                    .isHidden = true } },
        { "file", { .description = "Path to the output PGN file", 
                    .isRequired = true, 
                    .defaultValue = "", 
                    .type = ValueType::ValidateOutputPath } },
        { "append", { .description = "Append to existing file instead of overwriting it", 
                    .isRequired = false, 
                    .defaultValue = false, 
                    .type = ValueType::Bool } },
        { "finished", { .description = "Only save finished games", 
                .isRequired = false, 
                .defaultValue = true, 
                .type = ValueType::Bool } },
        { "min", { .description = "Only save minimal tag information in the PGN output", 
                .isRequired = false, 
                .defaultValue = false, 
                .type = ValueType::Bool } },
        { "clock", { .description = "Include clock information in the PGN output", 
                    .isRequired = false, 
                    .defaultValue = true, 
                    .type = ValueType::Bool } },
        { "eval", { .description = "Include evaluation values in the PGN output", 
                    .isRequired = false, 
                    .defaultValue = true, 
                    .type = ValueType::Bool } },
        { "depth", { .description = "Include search depth in the PGN output", 
                    .isRequired = false, 
                    .defaultValue = true, 
                    .type = ValueType::Bool } },
        { "pv", { .description = "Include principal variation in the PGN output", 
                .isRequired = false, 
                .defaultValue = false, 
                .type = ValueType::Bool } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getTournamentKeys() {
    return {
        { "id",   { .description = "Identifier for the configuration", 
                    .isRequired = false, 
                    .defaultValue = "tournament", 
                    .type = ValueType::String,
                    .isHidden = true } },
        { "type", { .description = "Tournament type: gauntlet/round-robin", 
                    .isRequired = true, 
                    .defaultValue = "gauntlet", 
                    .type = ValueType::String } },
        { "file", { .description = "Tournament stat file to load and update tournament state", 
                    .isRequired = false, 
                    .defaultValue = "", 
                    .type = ValueType::ValidateOutputPath } },
        { "saveintervalS", { .description = "Interval in seconds to save tournament state", 
                            .isRequired = false, 
                            .defaultValue = 10, 
                            .type = ValueType::UInt } },
        { "append", { .description = "Append to result file instead of overwriting it", 
                    .isRequired = false, 
                    .defaultValue = false, 
                    .type = ValueType::Bool } },
        { "event", { 
                    .description = "Optional event name for PGN or logging", 
                    .isRequired = false, 
                    .defaultValue = "", 
                    .type = ValueType::String } },
        { "games", { 
                    .description = "Number of games per pairing (total games = games * rounds)", 
                    .isRequired = false, 
                    .defaultValue = 2, 
                    .type = ValueType::UInt } },
        { "rounds", { .description = "Repeat all pairings this many times", 
                    .isRequired = false, 
                    .defaultValue = 1, 
                    .type = ValueType::UInt } },
        { "repeat", { .description = "Number of consecutive games using same opening (e.g. 2 with swapping colors)", 
                    .isRequired = false, 
                    .defaultValue = 2, 
                    .type = ValueType::UInt } },
        { "noswap", { .description = "Disable automatic color swap after each game", 
                    .isRequired = false, 
                    .defaultValue = false, 
                    .type = ValueType::Bool } },
        { "ratinginterval", { .description = "Interval (in games) for printing rating table", 
                            .isRequired = false, 
                            .defaultValue = 100, 
                            .type = ValueType::UInt } },
        { "averageelo", { .description = "Set average Elo level for scaling rating output", 
                        .isRequired = false, 
                        .defaultValue = 2600, 
                        .type = ValueType::Int } },
        { "outcomeinterval", { .description = "Interval (in games) for printing outcome table", 
                            .isRequired = false, 
                            .defaultValue = 0, 
                            .type = ValueType::UInt } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getDrawAdjudicationKeys() {
    return {
        { "id",         { 
            .description = "Identifier for the configuration", 
            .isRequired = false, 
            .defaultValue = "all", 
            .type = ValueType::String,
            .isHidden = true } },
        { "active",   { 
            .description = "Enable draw adjudication", 
            .isRequired = false, 
            .defaultValue = true, 
            .type = ValueType::Bool,
            .isHidden = true } },                    
        { "movenumber", { 
            .description = "Minimum number of full moves before draw adjudication can occur", 
            .longDescription = "The specific move number where draw adjudication becomes active. Before this move number, no draw adjudication will occur.",
            .isRequired = false, 
            .defaultValue = 60, 
            .type = ValueType::UInt } },
        { "movecount",  { 
            .description = "Required number of consecutive moves with evaluation in range", 
            .longDescription = "The number of consecutive moves where the evaluation must remain distinctively within the draw score range to trigger a draw adjudication.",
            .isRequired = false, 
            .defaultValue = 20, 
            .type = ValueType::UInt } },
        { "score",      { 
            .description = "Centipawn score range (+/-) around zero for draw adjudication", 
            .longDescription = "The score threshold in centipawns. If the evaluation of both engines stays within +/- this score for 'movecount' moves, the game is adjudicated as a draw.",
            .isRequired = false, 
            .defaultValue = 20, 
            .type = ValueType::Int } },
        { "test",       { 
            .description = "If true, only reports what would be adjudicated without taking action", 
            .longDescription = "When enabled, it reports potential draw adjudications without terminating the game. Useful for tuning draw settings.",
            .isRequired = false, 
            .defaultValue = false, 
            .type = ValueType::Bool } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getResignAdjudicationKeys() {
    return {
        { "id", { 
            .description = "Identifier for the configuration", 
            .isRequired = false, 
            .defaultValue = "all", 
            .type = ValueType::String,
            .isHidden = true } },
        { "active",   { 
            .description = "Enable resignation adjudication", 
            .isRequired = false, 
            .defaultValue = true, 
            .type = ValueType::Bool,
            .isHidden = true } },
        { "movecount", { 
            .description = "Required number of consecutive moves with score below threshold for resignation", 
            .longDescription = "Number of consecutive moves that must maintain a score below the specified threshold to trigger a resignation. A typical value is 5. Setting this to 0 disables resignation adjudication.",   
            .isRequired = true, 
            .defaultValue = 5, 
            .type = ValueType::UInt } },
        { "score",     { 
            .description = "Centipawn score below zero that triggers resignation", 
            .longDescription = "The evaluation threshold in centipawns that triggers a resignation. This value describes the margin by which a side must be falling behind. For example, a value of 500 means the evaluation must be <= -500 cp.",
            .isRequired = false, 
            .defaultValue = 500, 
            .type = ValueType::Int } },
        { "twosided",  { 
            .description = "If true, both sides must meet respective score conditions", 
            .longDescription = "If enabled, both engines must agree on the resignation condition (i.e. one sees itself losing, the other sees itself winning). This reduces false positives but may delay adjudication.",
            .isRequired = false, 
            .defaultValue = false, 
            .type = ValueType::Bool } },
        { "test", { 
            .description = "If true, only reports what would be adjudicated without taking action", 
            .longDescription = "When enabled, the system records statistics on how many games would have been adjudicated, the potential time saved, and any incorrect adjudications, without actually stopping the games. This allows you to test the impact of resignation settings safely.", 
            .isRequired = false, 
            .defaultValue = false, 
            .type = ValueType::Bool } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getSpsaKeys() {
    return {
        { "id",            { .description = "Identifier for the configuration", 
                            .isRequired = false, 
                            .defaultValue = "spsa", 
                            .type = ValueType::String,
                            .isHidden = true } },
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
                            .defaultValue = 20000, 
                            .type = ValueType::UInt } },
        { "outcomeinterval", { .description = "Interval in completed pairs for status and outcome output", 
            .isRequired = false, 
            .defaultValue = 50, 
            .type = ValueType::UInt } },
        { "seed",          { .description = "Random seed for opening selection", 
                            .isRequired = false, 
                            .defaultValue = 0, 
                            .type = ValueType::UInt } },
        { "noswap",        { .description = "Disable automatic color swap between games", 
                            .isRequired = false, 
                            .defaultValue = false, 
                            .type = ValueType::Bool } }
    };
}

QaplaHelpers::StableMap<std::string, ParameterDefinition> getSpsaValueKeys() {
    return {
        { "id",        { .description = "Identifier for the configuration", 
                        .isRequired = false, 
                        .defaultValue = "spsa", 
                        .type = ValueType::String,
                        .isHidden = true } },
        { "name",      { .description = "UCI parameter name to optimize", 
                        .longDescription = "The exact name of the UCI option as reported by the engine (e.g., 'Contempt', 'King Safety').",
                        .isRequired = true, 
                        .defaultValue = "", 
                        .type = ValueType::String } },
        { "default",   { .description = "Starting value for the parameter", 
                        .longDescription = "The initial value from which the optimization starts.",
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } },
        { "min",       { .description = "Minimum allowed value for the parameter", 
                        .longDescription = "The lower bound for the parameter. The optimizer will not suggest values below this.",
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } },
        { "max",       { .description = "Maximum allowed value for the parameter", 
                        .longDescription = "The upper bound for the parameter. The optimizer will not suggest values below this.",
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } },
        { "step",      { .description = "Perturbation step size (c_i in SPSA algorithm)", 
                        .longDescription = "The amount by which the parameter is changed in each direction to estimate the gradient. A rule of thumb is about 5-10% of the expected parameter range.",
                        .isRequired = true, 
                        .defaultValue = 0.0F, 
                        .type = ValueType::Float } }
    };
}

} // namespace QaplaTester::Settings
