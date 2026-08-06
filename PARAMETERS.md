# Qapla Engine Tester - Parameter Reference

> **This file is generated - do not edit it by hand.**
> It is written directly from the parameter definitions built into the program,
> so it always matches the version it was generated with.
>
> Regenerate it with:
> `qapla-engine-tester --markdown > PARAMETERS.md`

## Global Parameters

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| --concurrency | <number> | *required* | Maximum number of concurrently running engines. Use 0 for automatic detection based on physical CPU cores. In auto mode the runtime uses max(1, physical cores - 1). If core detection fails, it falls back to 1. |
| --enginesfile | <path> |  | Path to an ini file with engine configurations |
| --interactive | <bool> | false | Enables interactive mode |
| --rapid | <bool> | false | Enables rapid mode (suppresses engine info lines) |
| --settingsfile | <path> |  | Path to a settings file in INI-style format |

## --clop

Runs CLOP (Confident Local Optimization) with weighted quadratic logistic regression.
The optimizer fits a local quadratic win model over sampled parameter vectors and updates a local design weight function.
New samples are drawn according to this weight function and evaluated against configured opponent engines.
IMPORTANT: You MUST define all optimized parameters using the 'clopvalue' group.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| activepairs | <number> | 8 | Maximum number of concurrent unfinished CLOP sample pairs |
| samples | <number> | 100 | Maximum number of CLOP samples |
| gamespersample | <number> | 8 | Number of games per sampled parameter vector |
| warmupsamples | <number> | 500 | Number of initial random samples before local fitting |
| outcomeinterval | <number> | 10 | Interval in completed samples for status and outcome output |
| trace | <bool> | false | Enable detailed CLOP trace output (diagnostics and signal tables) |
| maxweightiterations | <number> | 25 | Maximum iterations for local weight refinement per sample |
| h | <number> | 3.0000 | Positive locality factor H in wk(x)=exp((q(x)-mu)/(H*sigma)). Smaller H makes regression more local. |
| priorvariance | <number> | 100.0000 | Gaussian prior variance for logistic regressions |
| seed | <number> | 0 | Random seed for sample generation |

## --clopvalue

Defines a parameter for CLOP optimization.
All fields (name, min, max) are mandatory.
Choose min/max so the search space contains only meaningful test values.
If you have a known baseline value (previous default/start value), set min/max so this value is approximately centered in the range.
Multiple 'clopvalue' groups can be defined to optimize several parameters simultaneously.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| name | string | *required* | The exact name of the UCI option as reported by the engine. |
| min | <number> | *required* | Minimum allowed value for the parameter |
| max | <number> | *required* | Maximum allowed value for the parameter |

## --draw

Configures global draw adjudication rules. These settings apply to all tournament modes (SPRT, Tournament, SPSA) unless explicitly overridden.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| movenumber | <number> | 60 | The specific move number where draw adjudication becomes active. Before this move number, no draw adjudication will occur. |
| movecount | <number> | 20 | The number of consecutive moves where the evaluation must remain distinctively within the draw score range to trigger a draw adjudication. |
| score | <number> | 20 | The score threshold in centipawns. If the evaluation of both engines stays within +/- this score for 'movecount' moves, the game is adjudicated as a draw. |
| test | <bool> | false | When enabled, it reports potential draw adjudications without terminating the game. Useful for tuning draw settings. |

## --each

Defines configuration options for all engines

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| dir | <path> | . | Working directory |
| proto | string | uci | Protocol (uci/xboard) |
| tc | string | 3+0.02 | Time control in format moves/time+inc or 'inf' |
| ponder | <bool> | false | Enable pondering, if the engine supports it |
| trace | string | command | Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work |
| restart | string | auto | Engine restart mode: auto (engine decides), on (always), or off (never) |
| option.[name] | string |  | UCI engine option |

## --engine

Defines an engine configuration

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| conf | string |  | Selects a pre-configured engine from the registry. You can see available engines via the 'manage_engines' tool. |
| name | string |  | Name of the engine |
| cmd | <path> |  | Directly specify the path to the engine executable. Use this if the engine is not in the registry. |
| dir | <path> |  | Working directory |
| proto | string |  | Protocol (uci/xboard) |
| tc | string |  | Time control in format moves/time+inc or 'inf' |
| ponder | <bool> |  | Enable pondering, if the engine supports it |
| gauntlet | <bool> | false | Set if engine is part of the gauntlet group. |
| trace | string |  | Sets the engine trace level (none/all/command). Requires that enginelog is enabled to work |
| restart | string |  | Engine restart mode: auto (engine decides), on (always), or off (never) |
| option.[name] | string |  | UCI engine option |

## --epd

Runs an EPD (Extended Position Description) testset.
Each engine analyzes a set of positions and its performance is measured by how many 'best moves' it finds within the configured search limits.
You can run time-limited analysis (maxtime/mintime/seenplies) or fixed search limits (depth or nodes).
Results are reported as a success rate and compared against a minimum threshold.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| file | <path> | *required* | Path and file name to the epd file |
| maxtime | <number> | 20 | Maximum allowed time in seconds per move during EPD analysis. Ignored when 'depth' or 'nodes' is greater than 0. |
| mintime | <number> | 2 | Minimum required time for an early stop, when a correct move is found. Ignored when 'depth' or 'nodes' is greater than 0. |
| seenplies | <number> | 0 | Amount of plies one of the expected moves must be shown to stop early (0 = off). Ignored when 'depth' or 'nodes' is greater than 0. |
| depth | <number> | 0 | Fixed search depth per position. If this value is greater than 0, EPD runs with a depth-limited search and ignores 'maxtime', 'mintime', and 'seenplies'. Value 0 means not set. Must not be combined with 'nodes' > 0. |
| nodes | <number> | 0 | Fixed node limit per position. If this value is greater than 0, EPD runs with a node-limited search and ignores 'maxtime', 'mintime', and 'seenplies'. Value 0 means not set. Must not be combined with 'depth' > 0. |
| minsuccess | <number> | 0 | If the success rate is below this threshold, the program will exit with code 13 (MissedTarget). |

## --logging

Logger configuration

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| engine | <bool> | true | If true, engine logging is enabled |
| path | <path> | . | Path to the logging directory |
| mode | string | one | Engine log file strategy: one (single file for all engines), each (separate file per engine) |
| trace | string | result | CLI logging level: none, result, all |
| mcp | string | result | MCP logging level: none, result, all |

## --mcp

Enables MCP mode and optional tool-name prefixing for parallel local/remote MCP server usage.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| prefix | string |  | Optional MCP tool name prefix used to avoid naming conflicts when multiple MCP servers are active. |

## --openings

Defines how start positions are selected

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| file | <path> | *required* | Path to file with opening positions |
| order | string | sequential | Order of position selection: random, sequential |
| srand | <number> | 5489 | Seed for random opening selection |
| plies | string | all | Max number of plies per opening (all = unlimited) |
| start | <number> | 1 | Index of first opening (1-based) |
| policy | string | default | Opening switch policy: default, encounter, round |

## --perft

Runs perft (performance test), counting the number of leaf nodes reached
after playing out all legal move sequences to a fixed depth. Used to verify move generator correctness
and speed. With 'divide' enabled (default), the node count is broken down per root move.
Root moves are distributed across up to 'concurrency' threads.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| position | string | startpos | Position to search from. Use 'startpos' for the standard initial position, or any other value is parsed as a FEN string. |
| depth | <number> | 1 | Search depth in plies |
| divide | <bool> | true | If true, prints the node count for each legal root move separately, in addition to the total. If false, only the total node count is printed. |
| showfen | <bool> | true | If true and 'divide' is enabled, the resulting FEN after each root move is printed alongside its node count. |

## --pgnoutput

PGN output settings

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| file | <path> | *required* | Path to the output PGN file |
| append | <bool> | false | Append to existing file instead of overwriting it |
| finished | <bool> | true | Only save finished games |
| min | <bool> | false | Only save minimal tag information in the PGN output |
| clock | <bool> | true | Include clock information in the PGN output |
| eval | <bool> | true | Include evaluation values in the PGN output |
| depth | <bool> | true | Include search depth in the PGN output |
| pv | <bool> | false | Include principal variation in the PGN output |

## --resign

Configures global resignation adjudication rules. These settings apply to all tournament modes (SPRT, Tournament, SPSA) unless explicitly overridden.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| movecount | <number> | *required* | Number of consecutive moves that must maintain a score below the specified threshold to trigger a resignation. A typical value is 5. Setting this to 0 disables resignation adjudication. |
| score | <number> | 500 | The evaluation threshold in centipawns that triggers a resignation. This value describes the margin by which a side must be falling behind. For example, a value of 500 means the evaluation must be <= -500 cp. |
| twosided | <bool> | false | If enabled, both engines must agree on the resignation condition (i.e. one sees itself losing, the other sees itself winning). This reduces false positives but may delay adjudication. |
| test | <bool> | false | When enabled, the system records statistics on how many games would have been adjudicated, the potential time saved, and any incorrect adjudications, without actually stopping the games. This allows you to test the impact of resignation settings safely. |

## --sprt

Runs SPRT (Sequential Probability Ratio Test).
Determines if Challenger is stronger than Baseline.
Roles:
- Challenger: Engine with 'gauntlet=true', or first engine if no gauntlet flag.
- Baseline: The other engine.
Hypotheses:
- H0 (Null): Challenger Elo <= Baseline + eloH0
- H1 (Alt):  Challenger Elo >= Baseline + eloH1
Configs:
- Improvement: eloH0=0, eloH1=5
- Regression: eloH0=-5, eloH1=0

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| file | <path> |  | File to load/save tournament outcome |
| saveintervals | <number> | 10 | Interval in seconds to save tournament state |
| eloh0 | <number> | 0.0000 | The Elo parameter for the null hypothesis (H0). If the result supports this hypothesis, we conclude that Engine 1's advantage is at most 'eloH0' Elo. |
| eloh1 | <number> | 5.0000 | The Elo parameter for the alternative hypothesis (H1). If the result supports this hypothesis, we conclude that Engine 1's advantage is at least 'eloH1' Elo. |
| alpha | <number> | 0.0500 | Type I error threshold |
| beta | <number> | 0.0500 | Type II error threshold |
| maxgames | <number> | *required* | Always set a limit of the maximum amount of games. Low limits are around 10000, a detailed analysis for small elo improvements (e.g. lower:0, uppder:3) would be best with limits around 50000 to 200000 games. |
| model | string | normalized | Model used for SPRT calculations normalized, logistic, bayesian |
| pentanomial | <bool> | true | Use pentanomial model for SPRT calculations |
| montecarlo | <bool> | false | Run Monte Carlo test instead of SPRT |

## --spsa

Optimizes engine parameters using the Simultaneous Perturbation Stochastic Approximation (SPSA) algorithm.
Parameters are perturbed in multiple iterations to find the optimal values that maximize playing strength.
The process requires two engines; the second engine will be automatically configured with the perturbed parameters.
Each iteration involves running a set of games with slightly different parameter values to estimate the gradient of the performance.
IMPORTANT: You MUST define ALL parameters you want to optimize using the 'spsavalue' group. 
Each 'spsavalue' must be fully defined with name, default, min, max, and step.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| activepairs | <number> | 32 | Maximum number of concurrent unfinished tournament pairs |
| learningrate | <number> | 0.0020 | Global learning rate for parameter updates (r in SPSA algorithm) |
| gamesperpair | <number> | 8 | Number of games per parameter perturbation pair |
| iterations | <number> | 20000 | Maximum number of optimization iterations |
| outcomeinterval | <number> | 50 | Interval in completed pairs for status and outcome output |
| seed | <number> | 0 | Random seed for opening selection |

## --spsavalue

Defines a parameter for SPSA optimization.
All fields (name, default, min, max, step) are mandatory for the optimization process.
Multiple 'spsavalue' groups can be defined to optimize several parameters simultaneously.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| name | string | *required* | The exact name of the UCI option as reported by the engine (e.g., 'Contempt', 'King Safety'). |
| default | <number> | *required* | The initial value from which the optimization starts. |
| min | <number> | *required* | The lower bound for the parameter. The optimizer will not suggest values below this. |
| max | <number> | *required* | The upper bound for the parameter. The optimizer will not suggest values below this. |
| step | <number> | *required* | The amount by which the parameter is changed in each direction to estimate the gradient. A rule of thumb is about 5-10% of the expected parameter range. |

## --systemtest

Evaluates how stable a platform allocates computation time to engines when running multiple games in parallel. Replays identical games at increasing concurrency levels and measures per-move NPS standard deviation. Helps determine the optimal number of concurrent games for tournament play on a given system.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| maxcores | <number> | 0 | Maximum number of parallel games to test. If set to 0, the resolved global concurrency value is used as upper bound. |
| step | <number> | 1 | How many concurrency slots are added after each step interval. |
| steptime | <number> | 30 | Number of seconds to run one concurrency step before increasing to the next step. |
| test | <bool> | false | If enabled, checks that in replay mode each computed move matches the expected search depth of the original game. |

## --test

Runs an extended test to evaluate engine stability and performance.         The test involves granular steps designed to trigger potential issues,      including crashes with invalid parameters. Each test type can be disabled;  all tests are on by default. Disable specific tests if a fast result is     more important than comprehensive coverage.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| underrun | <bool> | false | Verifies that the engine does not stop searching too early before the allocated time is used up. |
| timeusage | <bool> | false | Measures and validates the engine's time management accuracy during actual games. |
| numgames | <number> | 20 | Specifies the number of games played by the engine against itself to test long-term stability. Values greater than 0 enable the multi-game self-play test (including parallel execution by configured concurrency). A value of 0 skips this test. |
| noponder | <bool> | false | Disables tests for the 'ponder' UCI command, which checks if the engine correctly thinks during the opponent's time. (long running, full engine vs. engine game ponder on) |
| noepd | <bool> | false | Disables the EPD test, where the engine must find best moves in specific chess positions within a time limit. |
| nomemory | <bool> | false | Disables the hash table test, which verifies if the engine correctly allocates and frees memory when the 'Hash' option is changed. |
| nooption | <bool> | false | Disables the parameter smoke test. WARNING: This test is very long-running and intentionally uses invalid or edge-case values to trigger crashes. Highly recommended to disable for quick checks. |
| nostop | <bool> | false | Disables testing of the 'stop' command to ensure the engine interrupts its search immediately. |
| nowait | <bool> | false | Disables the infinite mode test, ensuring the engine does not prematurely exit its search when no limits are set. |
| noanalyze | <bool> | false | Disables the standard analysis test, verifying basic move generation and evaluation. |
| nogolimits | <bool> | false | Disables tests for specific search limits like 'depth', 'nodes', and 'movetime'. |
| nofens | <bool> | false | Disables testing of move generation from varied FEN positions including En Passant and castling rights. |
| nocompute | <bool> | false | Disables the single self-play game test used to verify end-to-end move flow. (long running, full engine vs. engine game) |

## --tournament

Runs a tournament between multiple engines.
Pairings are generated based on the tournament type (e.g., round-robin or gauntlet).
Engines play against each other with color swapping and opening variations.

| Parameter | Type | Default | Description |
| --- | --- | --- | --- |
| type | string | *required* | Tournament type: gauntlet/round-robin |
| file | <path> |  | Tournament stat file to load and update tournament state |
| saveintervals | <number> | 10 | Interval in seconds to save tournament state |
| append | <bool> | false | Append to result file instead of overwriting it |
| event | string |  | Optional event name for PGN or logging |
| games | <number> | 2 | Number of games per pairing (total games = games * rounds) |
| rounds | <number> | 1 | Repeat all pairings this many times |
| repeat | <number> | 2 | Number of consecutive games using same opening (e.g. 2 with swapping colors) |
| noswap | <bool> | false | Disable automatic color swap after each game |
| ratinginterval | <number> | 100 | Interval (in games) for printing rating table |
| averageelo | <number> | 2600 | Set average Elo level for scaling rating output |
| outcomeinterval | <number> | 0 | Interval (in games) for printing outcome table |

