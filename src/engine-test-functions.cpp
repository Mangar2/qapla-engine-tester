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

#include "engine-test-functions.h"
#include "engine-worker-factory.h"
#include "engine-report.h"
#include "logger.h"
#include "timer.h"
#include "compute-task.h"
#include "time-control.h"
#include "game-record.h"
#include "event-sink-recorder.h"
#include "game-manager.h"
#include "epd-test-manager.h"
#include "test-tournament.h"
#include "game-manager-pool.h"

#include <memory>
#include <sstream>
#include <iomanip>
#include <thread>

namespace QaplaTester {

static std::string bytesToMB(uint64_t bytes) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0));
    return oss.str();
}

template<typename TestCallback>
TestResult runTest(
    const std::vector<EngineConfig>& engineConfigs,
    TestCallback&& testCallback
)
{
    EngineList engines;
    
    try {
        // Start engines if configurations are provided
        if (!engineConfigs.empty()) {
            for (const auto& config : engineConfigs) {
                auto engineList = EngineWorkerFactory::createEngines(config, 1);
                for (auto& engine : engineList) {
                    engines.push_back(std::move(engine));
                }
            }
            
            // Verify engines respond to UCI isReady command to ensure they are operational
            for (const auto& engine : engines) {
                auto* checklist = EngineReport::getChecklist(engine->getEngineName());
                bool isReady = engine->requestReady();
                checklist->logReport("starts-and-stops-cleanly", isReady, 
                    "  engine did not respond to isReady after startup in time");
                if (!isReady) {
                    Logger::testLogger().log("Engine " + engine->getEngineName() + 
                        " did not start successfully", TraceLevel::error);
                }
            }
        }
        
        // Execute test callback with engine list (may be empty)
        TestResult result = testCallback(std::move(engines));
        
        // Engines will be stopped automatically by their destructors when leaving scope
        
        return result;
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during test execution: " + std::string(e.what()), 
            TraceLevel::error);
        
        // Engines will be stopped automatically by destructors during stack unwinding
        
        return {TestResultEntry("Error", std::string(e.what()), false)};
    }
}

TestResult runEngineStartStopTest(const EngineConfig& engineConfig)
{
    // Cannot use runTest() here because we need to measure startup time before engine is ready
    auto* checklist = EngineReport::getChecklist(engineConfig.getName());
    
    QaplaHelpers::Timer timer;
    timer.start();
    uint64_t startTime = 0;
    uint64_t stopTime = 0;
    uint64_t memoryInBytes = 0;
    std::string engineName;
    std::string engineAuthor;
    
    try {
        {
            auto engineList = EngineWorkerFactory::createEngines(engineConfig, 1);
            startTime = timer.elapsedMs();
            
            if (engineList.empty()) {
                return {TestResultEntry("Start/Stop timing", "No engine started", false)};
            }
            
            auto* engine = engineList[0].get();
            
            // Verify engine responds to UCI isReady command
            bool isReady = engine->requestReady();
            checklist->logReport("starts-and-stops-cleanly", isReady, 
                "  engine did not respond to isReady after startup in time");
            
            if (!isReady) {
                Logger::testLogger().log("Engine " + engineConfig.getName() + 
                    " did not start successfully", TraceLevel::error);
                return {TestResultEntry("Start/Stop timing", "Engine did not respond to isReady", false)};
            }
            
            // Get engine information
            engineName = engine->getEngineName();
            engineAuthor = engine->getEngineAuthor();
            memoryInBytes = engine->getEngineMemoryUsage();
            
            checklist->setAuthor(engineAuthor);
            
            Logger::testLogger().logAligned("Engine startup test:",
                "Name: " + engineName + ", Author: " + engineAuthor);
            
            // Measure shutdown time by letting engineList go out of scope
            timer.start();
        }
        stopTime = timer.elapsedMs();
        
        std::string timingInfo = "Started in " + std::to_string(startTime) + " ms, shutdown in " +
            std::to_string(stopTime) + " ms, memory usage " + bytesToMB(memoryInBytes) + " MB";
        
        Logger::testLogger().logAligned("Start/Stop timing:", timingInfo);
        
        return {TestResultEntry("Start/Stop timing", timingInfo, true)};
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during start/stop test: " + std::string(e.what()), 
            TraceLevel::error);
        return {TestResultEntry("Start/Stop timing", std::string(e.what()), false)};
    }
}

TestResult runEngineMultipleStartStopTest(const EngineConfig& engineConfig, uint32_t numEngines)
{
    auto* checklist = EngineReport::getChecklist(engineConfig.getName());
    
    QaplaHelpers::Timer timer;
    timer.start();
    uint64_t startTime = 0;
    uint64_t stopTime = 0;
    
    try {
        {
            EngineList engines = EngineWorkerFactory::createEngines(engineConfig, numEngines);
            startTime = timer.elapsedMs();
            
            // Engines stop automatically when leaving scope via destructors
        }
        stopTime = timer.elapsedMs();
        
        std::string timingInfo = "Started in " + std::to_string(startTime) + " ms, shutdown in " +
            std::to_string(stopTime) + " ms";
        
        Logger::testLogger().logAligned("Parallel start/stop (" + std::to_string(numEngines) + "):", timingInfo);
        
        bool success = startTime < 2000 && stopTime < 5000;
        if (!success) {
            checklist->logReport("starts-and-stops-cleanly", false,
                "  Start/Stop takes too long, started in: " + std::to_string(startTime) + 
                " ms, shutdown in " + std::to_string(stopTime) + " ms");
        }
        
        return {TestResultEntry("Parallel start/stop (" + std::to_string(numEngines) + ")", timingInfo, success)};
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during multiple start/stop test: " + std::string(e.what()), 
            TraceLevel::error);
        return {TestResultEntry("Parallel start/stop (" + std::to_string(numEngines) + ")", std::string(e.what()), false)};
    }
}

TestResult runHashTableMemoryTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("Hash table memory test", "No engine started", false)};
        }
        
        auto* engine = engines[0].get();
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        
        // Set Hash to 512MB and measure memory
        engine->setOption("Hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t memHigh = engine->getEngineMemoryUsage();
        
        // Set Hash to 16MB and measure memory
        engine->setOption("Hash", "16");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t memLow = engine->getEngineMemoryUsage();
        
        // Verify memory shrinkage of at least 400MB
        bool success = memLow + 400'000'000 < memHigh;
        
        // Reset to default
        engine->setOption("Hash", "32");
        
        std::string resultMsg = "Usage with 512MB hash " + bytesToMB(memHigh) + 
            " MB and with 16MB hash " + bytesToMB(memLow) + " MB" +
            (success ? " (shrinked)" : " (did not shrink enough)");
        
        Logger::testLogger().logAligned("Hash table memory test:", resultMsg);
        checklist->logReport("shrinks-with-hash", success, "  " + resultMsg);
        
        return {TestResultEntry("Hash table memory test", resultMsg, success)};
    });
}

TestResult runLowerCaseOptionTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("Lowercase option test", "No engine started", false)};
        }
        
        auto* engine = engines[0].get();
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        
        // Set lowercase 'hash' to 512 and measure memory
        engine->setOption("hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t lMem = engine->getEngineMemoryUsage();
        
        // Set uppercase 'Hash' to 512 and measure memory
        engine->setOption("Hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t uMem = engine->getEngineMemoryUsage();
        
        // Verify memory is similar (within 1000 bytes)
        bool success = (lMem + 1000 > uMem && lMem - 1000 < uMem);
        
        std::string resultMsg = std::string("Tried \"setoption name hash value 512\", ") +
            (success ? "lowercase option is accepted" : "lowercase option is not accepted");
        
        Logger::testLogger().logAligned("Lowercase option test:", resultMsg);
        checklist->logReport("lower-case-option", success, "  " + resultMsg);
        
        return {TestResultEntry("Lowercase option test", resultMsg, success)};
    });
}

// Helper functions for generating test values for different option types
namespace {

std::vector<std::string> generateCheckValues() {
    return { "true", "false" };
}

std::vector<std::string> generateSpinValues(const EngineOption& opt) {
    std::vector<std::string> values;
    if (opt.min && opt.max) {
        int min = *opt.min;
        int max = *opt.max;
        int mid = min + (max - min) / 2;
        values = {
            std::to_string(min),
            std::to_string(max),
            std::to_string(mid)
        };
    }
    else {
        values = { "0", "100", "-1" };
    }
    return values;
}

std::vector<std::string> generateComboValues(const EngineOption& opt) {
    std::vector<std::string> values = opt.vars;
    values.emplace_back("invalid_option");
    return values;
}

std::vector<std::string> generateStringValues() {
    return {
        "",
        "öäüß",               
        "C:\\invalid\\path",
        std::string(1024, 'A'), 
        "\x01\x02\x03\xFF"      
    };
}

} // anonymous namespace

// Helper function to test setting an option with proper error checking
static std::pair<bool, std::string> testSetOption(EngineWorker* engine, const std::string& name, const std::string& value) {
    bool success = engine->setOption(name, value);

    if (success) {
        return { true, "" };
    }

    bool failure = engine->failure();
    if (!failure && !engine->requestReady(std::chrono::seconds{ 10 })) {
        failure = true;
    }

    if (failure) {
        return { false, "Engine crashed or became unresponsive after setting option '" + name + "' to '" + value + "'" };
    }

    return { false, "Engine timed out after setting option '" + name + "' to '" + value + "'" };
}

TestResult runEngineOptionTests(const EngineConfig& engineConfig) // NOLINT(readability-function-cognitive-complexity)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("Engine option tests", "No engine started", false)};
        }
        
        auto* engine = engines[0].get();
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        const EngineOptions& options = engine->getSupportedOptions();
        
        int errors = 0;
        
        std::cout << "Randomizing engine settings, please wait...\r";
        for (const auto& opt : options) {
            if (opt.name == "Hash" || opt.type == EngineOption::Type::Button) {
                continue;
            }

            std::vector<std::string> testValues;

            switch (opt.type) {
            case EngineOption::Type::Check:
                testValues = generateCheckValues();
                break;
            case EngineOption::Type::Spin:
                testValues = generateSpinValues(opt);
                break;
            case EngineOption::Type::Combo:
                testValues = generateComboValues(opt);
                break;
            case EngineOption::Type::String:
                testValues = generateStringValues();
                break;
            default:
                continue;
            }

            for (const auto& value : testValues) {
                const std::string testName = "Option '" + opt.name + "' = '" + value + "'";
                const auto [success, message] = testSetOption(engine, opt.name, value);
                
                // Log individual test result
                checklist->logReport("options-safe", success, message);
                
                if (!success) {
                    errors++;
                    Logger::testLogger().log(std::format("Option test failed: {} - {}", testName, message), 
                        TraceLevel::error);
                }

                if (errors > 5) {
                    Logger::testLogger().log("Too many errors occurred, stopping further setoption tests.", TraceLevel::error);
                    std::string errorMsg = "Too many errors (" + std::to_string(errors) + 
                        ") after testing option values";
                    return {TestResultEntry("Engine option tests", errorMsg, false)};
                }
            }

            // Test resetting to default value
            if (!opt.defaultValue.empty()) {
                const std::string testName = "Option '" + opt.name + "' reset to default";
                const auto [success, message] = testSetOption(engine, opt.name, opt.defaultValue);
                
                // Log individual test result
                checklist->logReport("options-safe", success, message);
                
                if (!success) {
                    errors++;
                    Logger::testLogger().log(std::format("Option reset test failed: {} - {}", testName, message), TraceLevel::error);
                }

                if (errors > 5) {
                    Logger::testLogger().log("Too many errors occurred, stopping further setoption tests.", TraceLevel::error);
                    std::string errorMsg = "Too many errors (" + std::to_string(errors) + 
                        ") after testing option values";
                    return {TestResultEntry("Engine option tests", errorMsg, false)};
                }
            }
        }

        bool success = errors == 0;
        std::string resultMsg = (success ? 
            "No issues encountered." : 
            std::to_string(errors) + " failures detected. See log for details.");
        
        Logger::testLogger().logAligned("Edge case options:", resultMsg);
        
        return {TestResultEntry("Engine option tests", resultMsg, success)};
    });
}

TestResult runAnalyzeTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("Analyze test", "No engine started", false)};
        }
        
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        
        // Create ComputeTask and initialize with engines
        auto computeTask = std::make_unique<ComputeTask>();
        computeTask->initEngines(std::move(engines));
        
        static constexpr auto ANALYZE_TEST_TIMEOUT = std::chrono::milliseconds(500);
        static constexpr auto LONGER_TIMEOUT = std::chrono::milliseconds(2000);
        
        TimeControl t;
        t.setInfinite();
        computeTask->setTimeControl(t);

        for (const auto& fen : {
            "r3r1k1/1pq2pp1/2p2n2/1PNn4/2QN2b1/6P1/3RPP2/2R3KB b - - 0 1",
            "r1q2rk1/p2bb2p/1p1p2p1/2pPp2n/2P1PpP1/3B1P2/PP2QR1P/R1B2NK1 b - - 0 1"
            }) {
            computeTask->newGame();
            computeTask->setPosition(false, fen);
            computeTask->computeMove();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            computeTask->moveNow();
            bool finished = computeTask->getFinishedFuture().wait_for(ANALYZE_TEST_TIMEOUT) == std::future_status::ready;
            if (!finished) {
                bool extended = computeTask->getFinishedFuture().wait_for(LONGER_TIMEOUT) == std::future_status::ready;
                if (!extended) {
                    Logger::testLogger().logAligned("Testing stop command:", "Timeout after stop command (even after extended wait)");
                    checklist->logReport("reacts-on-stop", false, "Timeout after stop command (even after extended wait)");
                    return {TestResultEntry("Analyze test", "Timeout after stop command", false)};
                }
            }
        }
        
        Logger::testLogger().logAligned("Testing stop command:", "Engine correctly handled stop command and sent bestmove");
        checklist->logReport("reacts-on-stop", true, "");
        
        return {TestResultEntry("Analyze test", "Engine correctly handled stop command and sent bestmove", true)};
    });
}

TestResult runImmediateStopTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("Immediate stop test", "No engine started", false)};
        }
        
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        
        // Create ComputeTask and initialize with engines
        auto computeTask = std::make_unique<ComputeTask>();
        computeTask->initEngines(std::move(engines));
        
        static constexpr auto ANALYZE_TEST_TIMEOUT = std::chrono::milliseconds(500);
        static constexpr auto LONGER_TIMEOUT = std::chrono::milliseconds(2000);
        
        TimeControl t;
        t.setInfinite();
        computeTask->setTimeControl(t);
        computeTask->setPosition(false, "3r1r2/pp1q2bk/2n1nppp/2p5/3pP1P1/P2P1NNQ/1PPB3P/1R3R1K w - - 0 1");
        computeTask->computeMove();
        computeTask->moveNow();
        bool finished = computeTask->getFinishedFuture().wait_for(ANALYZE_TEST_TIMEOUT) == std::future_status::ready;
        if (!finished) {
            bool extended = computeTask->getFinishedFuture().wait_for(LONGER_TIMEOUT) == std::future_status::ready;
            if (!extended) {
                Logger::testLogger().logAligned("Testing immediate stop:", "Timeout after immediate stop");
                checklist->logReport("correct-after-immediate-stop", false, "Timeout after immediate stop");
                return {TestResultEntry("Immediate stop test", "Timeout after immediate stop", false)};
            }
        }
        
        Logger::testLogger().logAligned("Testing immediate stop:", "Engine correctly handled immediate stop and sent bestmove");
        checklist->logReport("correct-after-immediate-stop", true, "");
        
        return {TestResultEntry("Immediate stop test", "Engine correctly handled immediate stop and sent bestmove", true)};
    });
}

TestResult runInfiniteAnalyzeTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("Infinite analyze test", "No engine started", false)};
        }
        
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        
        // Create ComputeTask and initialize with engines
        auto computeTask = std::make_unique<ComputeTask>();
        computeTask->initEngines(std::move(engines));
        
        static constexpr auto LONGER_TIMEOUT = std::chrono::milliseconds(2000);
        static constexpr auto NO_BESTMOVE_TIMEOUT = std::chrono::milliseconds(10000);
        
        std::cout << "Testing infinite mode: takes about 10 seconds, please wait...";
        std::cout.flush();
        std::cout << "\r";
        
        TimeControl t;
        t.setInfinite();
        computeTask->setTimeControl(t);
        computeTask->setPosition(false, "K7/8/k7/8/8/8/8/3r4 b - - 0 1");
        computeTask->computeMove();
        bool exited = computeTask->getFinishedFuture().wait_for(NO_BESTMOVE_TIMEOUT) == std::future_status::ready;
        if (exited) {
            Logger::testLogger().logAligned("Testing infinite mode:", "Engine sent bestmove without receiving 'stop'", TraceLevel::command);
            checklist->logReport("infinite-move-does-not-exit", false, "Engine sent bestmove in infinite mode without receiving 'stop'");
            return {TestResultEntry("Infinite analyze test", "Engine sent bestmove without receiving 'stop'", false)};
        }
        computeTask->moveNow();
        bool stopped = computeTask->getFinishedFuture().wait_for(LONGER_TIMEOUT) == std::future_status::ready;
        if (!stopped) {
            Logger::testLogger().logAligned("Testing infinite mode:", "Timeout after stop command", TraceLevel::command);
            checklist->logReport("infinite-move-does-not-exit", false, "Timeout after stop command in infinite mode");
            return {TestResultEntry("Infinite analyze test", "Timeout after stop command in infinite mode", false)};
        }
        computeTask->getFinishedFuture().wait();
        
        Logger::testLogger().logAligned("Testing infinite mode:", "Correctly waited for stop and then sent bestmove", TraceLevel::command);
        checklist->logReport("infinite-move-does-not-exit", true, "");
        
        return {TestResultEntry("Infinite analyze test", "Correctly waited for stop and then sent bestmove", true)};
    });
}

TestResult runGoLimitsTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("Go limits test", "No engine started", false)};
        }
        
        auto computeTask = std::make_unique<ComputeTask>();
        computeTask->initEngines(std::move(engines));
        
        static constexpr auto GO_TIMEOUT = std::chrono::seconds(4);
        
        struct TestCase {
            std::string name;
            TimeControl timeControl;
        };
        
        std::vector<TestCase> testCases = {
            {.name="no-loss-on-time", 
                .timeControl=[] { TimeControl t; t.addTimeSegment({.movesToPlay=0, .baseTimeMs=1000, .incrementMs=500}); return t; }()},
            {.name="no-loss-on-time", 
                .timeControl=[] { TimeControl t; t.addTimeSegment({.movesToPlay=0, .baseTimeMs=100, .incrementMs=2000}); return t; }()},
            {.name="supports-movetime", 
                .timeControl=[] { TimeControl t; t.setMoveTime(1000); return t; }()},
            {.name="supports-depth-limit", 
                .timeControl=[] { TimeControl t; t.setDepth(4); return t; }()},
            {.name="supports-node-limit", 
                .timeControl=[] { TimeControl t; t.setNodes(10000); return t; }()}
        };
        
        TestResult results;
        int errors = 0;
        
        for (const auto& testCase : testCases) {
            computeTask->newGame();
            computeTask->setTimeControl(testCase.timeControl);
            computeTask->setPosition(true);
            computeTask->computeMove();
            bool success = computeTask->getFinishedFuture().wait_for(GO_TIMEOUT) == std::future_status::ready;
            
            if (!success) {
                computeTask->moveNow();
                errors++;
            }
            
            bool finished = computeTask->getFinishedFuture().wait_for(GO_TIMEOUT) == std::future_status::ready;
            if (!finished) {
                computeTask->stop();
            }
            
            auto timeStr = testCase.timeControl.toPgnTimeControlString();
            if (!timeStr.empty()) {
                timeStr.insert(0, " Time control: ");
            }
            
            std::string result = success ? "OK" : "Timeout";
            results.emplace_back(testCase.name, result + timeStr, success);
        }
        
        Logger::testLogger().logAligned("Testing go limits:", 
            errors == 0 ? "All limits work correctly" : std::to_string(errors) + " errors");
        
        return results;
    });
}

TestResult runEpFromFenTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("EP from FEN test", "No engine started", false)};
        }
        
        auto computeTask = std::make_unique<ComputeTask>();
        computeTask->initEngines(std::move(engines));
        
        TimeControl timeControl;
        timeControl.addTimeSegment({ .movesToPlay = 0, .baseTimeMs = 1000, .incrementMs = 100 });
        computeTask->setTimeControl(timeControl);
        computeTask->setPosition(false, "rnbqkb1r/ppp2ppp/8/3pP3/4n3/5N2/PPP2PPP/RNBQKB1R w KQkq d6 0 1",
            std::vector<std::string>{"e5d6"});
        computeTask->computeMove();
        bool finished = computeTask->getFinishedFuture().wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        
        Logger::testLogger().logAligned("Testing EP from FEN:", 
            finished ? "Position handled correctly" : "Timeout");
        
        return {TestResultEntry("EP from FEN test", 
            finished ? "Position handled correctly" : "Timeout", finished)};
    });
}

TestResult runComputeGameTest(const EngineConfig& engineConfig, bool logMoves)
{
    return runTest({engineConfig, engineConfig}, [logMoves](EngineList&& engines) -> TestResult {
        if (engines.size() < 2) {
            return {TestResultEntry("Compute game test", "Could not start two engines", false)};
        }
        
        auto computeTask = std::make_unique<ComputeTask>();
        computeTask->initEngines(std::move(engines));
        
        try {
            computeTask->newGame();
            computeTask->setPosition(true);
            TimeControl t1; t1.addTimeSegment({ .movesToPlay = 0, .baseTimeMs = 20000, .incrementMs = 100 });
            TimeControl t2; t2.addTimeSegment({ .movesToPlay = 0, .baseTimeMs = 10000, .incrementMs = 100 });
            computeTask->setTimeControls({t1, t2});
            computeTask->autoPlay(logMoves);
            computeTask->getFinishedFuture().wait();
            
            Logger::testLogger().logAligned("Testing game play:", "Game completed successfully");
            return {TestResultEntry("Compute game test", "Game completed successfully", true)};
        }
        catch (const std::exception& e) {
            Logger::testLogger().logAligned("Testing game play:", std::string("Error: ") + e.what());
            return {TestResultEntry("Compute game test", std::string("Error: ") + e.what(), false)};
        }
    });
}

// Helper function to test ponder hit scenario
static void testPonderHit(const GameRecord& gameRecord, EngineWorker* engine,
    const std::string& ponderMove, const std::string& testname,
    std::chrono::milliseconds sleep = std::chrono::milliseconds(0)) {
    static constexpr auto TIMEOUT = std::chrono::milliseconds(2000);

    auto* checklist = EngineReport::getChecklist(engine->getConfig().getName());
    EventSinkRecorder recorder;
    engine->setEventSink(recorder.getCallback());
    engine->newGame(gameRecord, gameRecord.isWhiteToMove());
    
    TimeControl t;
    t.addTimeSegment({ .movesToPlay = 0, .baseTimeMs = 2000, .incrementMs = 0 });
    GoLimits goLimits = createGoLimits(t, t, 0, 0, 0, true);
    
    engine->allowPonder(gameRecord, goLimits, ponderMove);
    std::this_thread::sleep_for(sleep);
    
    bool success = recorder.count(EngineEvent::Type::BestMove) == 0;
    checklist->logReport(testname, success, "Engine sent a bestmove while in ponder mode. ");
    
    engine->setWaitForHandshake(EngineEvent::Type::BestMove);
    engine->computeMove(gameRecord, goLimits, true);
    success = engine->waitForHandshake(TIMEOUT);
    checklist->logReport(testname, success, "Engine did not send a bestmove after compute move in ponder mode.");
}

// Helper function to test ponder miss scenario
static void testPonderMiss(const GameRecord& gameRecord, EngineWorker* engine,
    const std::string& ponderMove, const std::string& testname,
    std::chrono::milliseconds sleep = std::chrono::milliseconds(100)) {
    static constexpr auto TIMEOUT = std::chrono::milliseconds(5000);
    auto* checklist = EngineReport::getChecklist(engine->getConfig().getName());

    EventSinkRecorder recorder;
    engine->setEventSink(recorder.getCallback());
    engine->newGame(gameRecord, gameRecord.isWhiteToMove());
    
    TimeControl t;
    t.addTimeSegment({ .movesToPlay = 0, .baseTimeMs = 2000, .incrementMs = 0 });
    GoLimits goLimits = createGoLimits(t, t, 0, 0, 0, true);
    
    engine->allowPonder(gameRecord, goLimits, ponderMove);
    std::this_thread::sleep_for(sleep);
    
    bool success = recorder.count(EngineEvent::Type::BestMove) == 0;
    checklist->logReport(testname, success, "Engine sent a bestmove while in ponder mode. ");
    
    success = engine->moveNow(true, std::chrono::milliseconds(500));
    checklist->logReport(testname, success, "Engine did not send a bestmove fast after receiving stop in ponder mode.");
    
    if (!success) {
        success = engine->waitForHandshake(TIMEOUT);
        checklist->logReport(testname, success, "Engine never sent a bestmove after receiving stop in ponder mode.");
    }
}

TestResult runUciPonderTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("UCI ponder test", "No engine started", false)};
        }
        
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        auto* engine = engines[0].get();
        
        const std::string testname = "correct-pondering";
        
        try {
            std::cout << "Testing pondering:\n" << std::flush;
            
            GameRecord gameRecord;
            
            // Test with initial position and "e2e4" ponder move
            testPonderHit(gameRecord, engine, "e2e4", testname);
            testPonderHit(gameRecord, engine, "e2e4", testname, std::chrono::milliseconds(0));
            testPonderMiss(gameRecord, engine, "e2e4", testname);
            testPonderMiss(gameRecord, engine, "e2e4", testname, std::chrono::milliseconds(0));
            
            // Test with different position and "h4h3" ponder move
            gameRecord.setStartPosition(false, "K7/8/8/4Q3/5Q1k/8/8/8 b - - 2 68", false, 0,
                engineConfig.getName(), engineConfig.getName());
            testPonderHit(gameRecord, engine, "h4h3", testname);
            testPonderHit(gameRecord, engine, "h4h3", testname, std::chrono::milliseconds(0));
            testPonderMiss(gameRecord, engine, "h4h3", testname);
            testPonderMiss(gameRecord, engine, "h4h3", testname, std::chrono::milliseconds(0));
            
            return {TestResultEntry("UCI ponder test", "All ponder scenarios tested", true)};
        }
        catch (const std::exception& e) {
            Logger::testLogger().log("Exception during uci ponder test: " + std::string(e.what()), TraceLevel::error);
            checklist->logReport(testname, false, "Exception during uci ponder test: " + std::string(e.what()));
            return {TestResultEntry("UCI ponder test", "Exception: " + std::string(e.what()), false)};
        }
        catch (...) {
            Logger::testLogger().log("Unknown exception during uci ponder test", TraceLevel::error);
            checklist->logReport(testname, false, "Unknown exception during uci ponder test");
            return {TestResultEntry("UCI ponder test", "Unknown exception", false)};
        }
    });
}

TestResult runPonderGameTest(const EngineConfig& engineConfig, bool logMoves)
{
    return runTest({engineConfig, engineConfig}, 
        [logMoves](EngineList&& engines) -> TestResult {
        if (engines.size() < 2) {
            return {TestResultEntry("Ponder game test", "Could not start two engines", false)};
        }
        
        // Enable pondering for both engines
        engines[0]->getConfigMutable().setPonder(true);
        engines[1]->getConfigMutable().setPonder(true);
        
        auto computeTask = std::make_unique<ComputeTask>();
        computeTask->initEngines(std::move(engines));
        
        try {
            Logger::testLogger().log("The engine now plays against itself with pondering enabled", TraceLevel::command);
            
            computeTask->newGame();
            computeTask->setPosition(true);
            TimeControl t1; t1.addTimeSegment({ .movesToPlay = 0, .baseTimeMs = 20000, .incrementMs = 100 });
            TimeControl t2; t2.addTimeSegment({ .movesToPlay = 0, .baseTimeMs = 10000, .incrementMs = 100 });
            computeTask->setTimeControls({t1, t2});
            computeTask->autoPlay(logMoves);
            computeTask->getFinishedFuture().wait();
            
            Logger::testLogger().logAligned("Testing ponder game:", "Game completed successfully");
            return {TestResultEntry("Ponder game test", "Game completed successfully", true)};
        }
        catch (const std::exception& e) {
            Logger::testLogger().logAligned("Testing ponder game:", std::string("Error: ") + e.what());
            return {TestResultEntry("Ponder game test", std::string("Error: ") + e.what(), false)};
        }
    });
}

TestResult runEpdTest(const EngineConfig& engineConfig)
{
    return runTest({engineConfig}, [&engineConfig](EngineList&& engines) -> TestResult {
        if (engines.empty()) {
            return {TestResultEntry("EPD test", "No engine started", false)};
        }
        
        auto* checklist = EngineReport::getChecklist(engineConfig.getName());
        
        try {
            Logger::testLogger().log("Testing positions, this will take a while...", TraceLevel::command);
            
            auto epdManager = std::make_shared<EpdTestManager>(checklist);
            GameManager gameManager(nullptr);
            gameManager.initUniqueEngine(std::move(engines[0]));
            gameManager.start(epdManager);
            gameManager.getFinishedFuture().wait();
            
            Logger::testLogger().logAligned("Testing positions:", "All positions computed");
            return {TestResultEntry("EPD test", "All positions computed successfully", true)};
        }
        catch (const std::exception& e) {
            Logger::testLogger().logAligned("Testing positions:", std::string("Error: ") + e.what());
            checklist->logReport("epd-test", false, "Exception during EPD test: " + std::string(e.what()));
            return {TestResultEntry("EPD test", std::string("Error: ") + e.what(), false)};
        }
        catch (...) {
            Logger::testLogger().logAligned("Testing positions:", "Unknown error");
            checklist->logReport("epd-test", false, "Unknown exception during EPD test");
            return {TestResultEntry("EPD test", "Unknown error", false)};
        }
    });
}

TestResult runMultipleGamesTest(const EngineConfig& engineConfig, uint32_t numGames, uint32_t concurrency)
{
    // This test doesn't use runTest() because it needs GameManagerPool
    // which has different lifecycle management than single engines
    auto* checklist = EngineReport::getChecklist(engineConfig.getName());
    
    try {
        Logger::testLogger().log("Testing playing " + std::to_string(numGames) + " games...", TraceLevel::command);
        
        auto tournament = std::make_shared<TestTournament>(numGames, checklist);
        
        GameManagerPool::getInstance().addTaskProvider(tournament, engineConfig, engineConfig);
        GameManagerPool::getInstance().setConcurrency(concurrency, true, true);  
        GameManagerPool::getInstance().waitForTaskPolling(std::chrono::seconds(1));

        Logger::testLogger().logAligned("Testing multiple games:", "All games completed");
        return {TestResultEntry("Multiple games test", "Completed " + std::to_string(numGames) + " games successfully", true)};
    }
    catch (const std::exception& e) {
        Logger::testLogger().logAligned("Testing multiple games:", std::string("Error: ") + e.what());
        checklist->logReport("multiple-games", false, "Exception during multiple games test: " + std::string(e.what()));
        return {TestResultEntry("Multiple games test", std::string("Error: ") + e.what(), false)};
    }
    catch (...) {
        Logger::testLogger().logAligned("Testing multiple games:", "Unknown error");
        checklist->logReport("multiple-games", false, "Unknown exception during multiple games test");
        return {TestResultEntry("Multiple games test", "Unknown error", false)};
    }
}

} // namespace QaplaTester
