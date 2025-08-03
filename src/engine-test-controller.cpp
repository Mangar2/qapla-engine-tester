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

#include <memory>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include "engine-test-controller.h"
#include "engine-worker-factory.h"
#include "engine-report.h"
#include "cli-settings-manager.h"
#include "epd-test-manager.h"
#include "compute-task.h"
#include "game-manager.h"
#include "game-manager-pool.h"
#include "event-sink-recorder.h"

void EngineTestController::createGameManager() {
    computeTask_ = std::make_unique<ComputeTask>();
    startEngine();
}

void EngineTestController::startEngine() {
    bool success = false;
    try {
        auto ctList = EngineWorkerFactory::createEngines(engineConfig_, 1);
		computeTask_->initEngines(std::move(ctList));
		success = computeTask_->getEngine()->requestReady();
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Configuration error during engine test for " + 
            engineConfig_.getName() + ": " + std::string(e.what()), 
            TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during engine test for " + engineConfig_.getName(), 
        TraceLevel::error);
    }
    checklist_->logReport("starts-and-stops-cleanly", success, "  engine did not respond to isReady after startup in time");
    if (!success) {
		Logger::testLogger().log("Engine did not start successfully", TraceLevel::error);
		throw(std::runtime_error("Engine did not start successfully"));
    }
}

EngineList EngineTestController::startEngines(uint32_t count) {
    EngineList list = EngineWorkerFactory::createEngines(engineConfig_, count);

    std::vector<std::future<bool>> results;
    for (auto& engine : list) {
        results.push_back(std::async(std::launch::async, [&engine]() {
            return engine->requestReady();
            }));
    }

    bool allReady = std::all_of(results.begin(), results.end(), [](auto& f) {
        return f.get();
        });

    checklist_->logReport("starts-and-stops-cleanly", allReady, "  one or more engines did not respond to isReady in time");
    if (!allReady) {
        Logger::testLogger().log("Engines did not start successfully", TraceLevel::error);
    }

    return list;
}

static std::string bytesToMB(uint64_t bytes) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0));
	return oss.str();
}

void EngineTestController::runAllTests(const EngineConfig& engine, int numGames) {
    engineConfig_ = engine;
	checklist_ = EngineReport::getChecklist(engineConfig_.getName());
    try {
        const CliSettings::GroupInstance testSettings = *CliSettings::Manager::getGroupInstance("test");
        numGames_ = numGames;
        createGameManager();
        runStartStopTest();
        runMultipleStartStopTest(20);
        if (!testSettings.get<bool>("nomemory")) {
            runHashTableMemoryTest();
        }
        if (!testSettings.get<bool>("nooption")) {
            runLowerCaseOptionTest();
            runEngineOptionTests();
        }
        runAnalyzeTest();
		if (!testSettings.get<bool>("nostop")) {
            runImmediateStopTest();
        }
		if (!testSettings.get<bool>("nowait")) {
            runInfiniteAnalyzeTest();
        }
        runGoLimitsTests();
        runEpFromFenTest();
        if (!testSettings.get<bool>("noepd")) {
			runEpdTests();
		}
        runComputeGameTest();
        if (!testSettings.get<bool>("noponder")) {
            runUciPonderTest();
            runPonderGameTest();
        }
        runMultipleGamesTest();
    }
	catch (const std::exception& e) {
		Logger::testLogger().log("Exception during engine tests, all remaining tests cancelled: " + std::string(e.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception during engine tests, all remaining tests cancelled.", TraceLevel::error);
	}

}

/**
 * Runs a standardized test sequence including pre-checks, initialization, and error handling.
 * @param testName A descriptive name of the test.
 * @param testCallback A callable returning std::pair<bool, std::string> indicating success and optional error message.
 */
void EngineTestController::runTest(
    const std::string& testName,
    const std::function<std::pair<bool, std::string>()>& testCallback)
{
    constexpr std::chrono::seconds timeout{ 2 };
    try {
        if (!computeTask_) {
			Logger::testLogger().log("ComputeTask not initialized", TraceLevel::error);
            return;
        }
        if (!computeTask_->getEngine()) {
            startEngine();
        }
		bool isComputeReady = computeTask_->getEngine()->requestReady(timeout);
        if (!isComputeReady) {
            startEngine();
		}

        const auto [success, errorMessage] = testCallback();
        if (testName != "") {
            checklist_->logReport(testName, success, errorMessage);
        }
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during test '" + testName + "': " + e.what(), TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during test '" + testName + "'", TraceLevel::error);
    }
}

void EngineTestController::runStartStopTest() {
    runTest("starts-and-stops-cleanly", [this]() -> std::pair<bool, std::string> {
        Timer timer;
        timer.start();
        auto list = EngineWorkerFactory::createEngines(engineConfig_, 1);
        auto startTime = timer.elapsedMs();
        auto engine = list[0].get();
        uint64_t memoryInBytes = engine->getEngineMemoryUsage();
        engine->stop();
        auto stopTime = timer.elapsedMs();

        startStopSucceeded = true;

        Logger::testLogger().logAligned("Engine startup test:",
            "Name: " + engine->getEngineName() + ", Author: " + engine->getEngineAuthor());

        checklist_->setAuthor(engine->getEngineAuthor());
        Logger::testLogger().logAligned("Start/Stop timing:", "Started in " + std::to_string(startTime) + " ms, shutdown in " +
            std::to_string(stopTime) + " ms, memory usage " + bytesToMB(memoryInBytes) + " MB");

        return { true, "" };
        });
    if (!startStopSucceeded) {
        Logger::testLogger().log("Engine could not be started or stopped. Skipping remaining tests.", TraceLevel::error);
        return;
    }
}

void EngineTestController::runMultipleStartStopTest(uint32_t numEngines) {
    runTest("starts-and-stops-cleanly", [this, numEngines]() -> std::pair<bool, std::string> {
        Timer timer;
        timer.start();
		uint64_t startTime = 0;
        {
            EngineList engines = EngineWorkerFactory::createEngines(engineConfig_, numEngines);
            startTime = timer.elapsedMs();
            for (auto& engine : engines) {
                engine->stop(false);
            }
        }
        auto stopTime = timer.elapsedMs();

        Logger::testLogger().logAligned("Parallel start/stop (" + std::to_string(numEngines) + "):", 
            "Started in " + std::to_string(startTime) + " ms, shutdown in " + std::to_string(stopTime) + " ms");

        return { startTime < 2000 && stopTime < 5000, 
            "Start/Stop takes too long, started in: " + std::to_string(startTime) + " ms, shutdown in " + std::to_string(stopTime) + " ms"};
    });
}


void EngineTestController::runGoLimitsTests() {
    static constexpr auto GO_TIMEOUT = std::chrono::seconds(4);
    struct TestCase {
        std::string name;
        TimeControl timeControl;
    };

    std::vector<std::pair<std::string, TimeControl>> testCases = {
        { "no-loss-on-time", [] {
            TimeControl t; t.addTimeSegment({0, 1000, 500}); return t;
        }() },
        { "no-loss-on-time", [] {
            TimeControl t; t.addTimeSegment({0, 100, 2000}); return t;
        }() },
        { "supports-movetime", [] { TimeControl t; t.setMoveTime(1000); return t; }() },
        { "supports-depth-limit", [] { TimeControl t; t.setDepth(4); return t; }() },
        { "supports-node-limit", [] { TimeControl t; t.setNodes(10000); return t; }() }
    };
	int errors = 0;
    for (const auto& [name, timeControl] : testCases) {
        runTest(name, [this, name, timeControl, &errors]() -> std::pair<bool, std::string> {
            computeTask_->newGame();
			computeTask_->setTimeControl(timeControl);
            computeTask_->setPosition(true);
            computeTask_->computeMove();
            bool success = computeTask_->getFinishedFuture().wait_for(GO_TIMEOUT) == std::future_status::ready;
			if (!success) {
                computeTask_->moveNow();
                errors++;
			}
            bool finished = computeTask_->getFinishedFuture().wait_for(GO_TIMEOUT) == std::future_status::ready;
			if (!finished) {
                computeTask_->stop();
			}
            auto timeStr = timeControl.toPgnTimeControlString();
			if (timeStr != "") {
				timeStr = " Time control: " + timeStr + "";
			}
            return { success, "Compute move did not return with bestmove in time when testing " + name + timeStr};
        });
    }
    Logger::testLogger().logAligned("Testing compute moves:", 
        errors == 0 ? "Time limits, node limits and depth limits works well": std::to_string(errors) + " errors");
}

void EngineTestController::runHashTableMemoryTest() {
    runTest("shrinks-with-hash", [this]() -> std::pair<bool, std::string> {
        setOption("Hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t memHigh = computeTask_->getEngine()->getEngineMemoryUsage();

        setOption("Hash", "16");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t memLow = computeTask_->getEngine()->getEngineMemoryUsage();

        bool success = memLow + 400'000'000 < memHigh;

        computeTask_->getEngine()->setOption("Hash", "32");

        Logger::testLogger().logAligned("Test if memory shrinks:", "Usage with 512MB hash " +
            bytesToMB(memHigh) + " MB and with 16MB hash " + bytesToMB(memLow) + " MB" +
            (success ? " (shrinked)" : " (did not shrink enough)"));

        std::string errorMsg;
        if (!success) {
            errorMsg = "Memory did not shrink as expected when changing Hash from 512 to 16. "
                "Usage was " + std::to_string(memHigh) + " bytes before, now " +
                std::to_string(memLow) + " bytes.";
        }

        return { success, errorMsg };
        });
}

void EngineTestController::runLowerCaseOptionTest() {
    runTest("lower-case-option", [this]() -> std::pair<bool, std::string> {
        setOption("hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t lMem = computeTask_->getEngine()->getEngineMemoryUsage();

        setOption("Hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::size_t uMem = computeTask_->getEngine()->getEngineMemoryUsage();

        bool success = (lMem + 1000 > uMem && lMem - 1000 < uMem);

        Logger::testLogger().logAligned("Test lowercase option:", std::string("Tried \"setoption name hash value 512\", ") +
            (success ? "lowercase option is accepted" : "lowercase option is not accepted"));

        return { success, success ? "" : "setoption is case sensitive."};
        });
}

static std::vector<std::string> generateCheckValues() {
    return { "true", "false" };
}

static std::vector<std::string> generateSpinValues(const EngineOption& opt) {
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

static std::vector<std::string> generateComboValues(const EngineOption& opt) {
    std::vector<std::string> values = opt.vars;
    values.push_back("invalid_option");
    return values;
}

static std::vector<std::string> generateStringValues() {
    return {
        "",
        "öäüß",               
        "C:\\invalid\\path",
        std::string(1024, 'A'), 
        "\x01\x02\x03\xFF"      
    };
}

std::pair<bool, std::string> EngineTestController::setOption(const std::string& name, const std::string& value) {
    auto engine = computeTask_->getEngine();
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


void EngineTestController::runEngineOptionTests() {
    int errors = 0;

    if (!computeTask_) {
        Logger::testLogger().log("GameManager not initialized", TraceLevel::error);
        return;
    }

    auto engine = computeTask_->getEngine();
    const EngineOptions& options = engine->getSupportedOptions();
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
            runTest("options-safe", [this, opt, value, &errors]() -> std::pair<bool, std::string> {
                const auto [success, message] = setOption(opt.name, value);
                if (!success) errors++;
                return { success, message };
                });

            if (errors > 5) {
                Logger::testLogger().log("Too many errors occurred, stopping further setoption tests.", TraceLevel::error);
                return;
            }
        }

        if (!opt.defaultValue.empty()) {
            const std::string testName = "Option '" + opt.name + "' reset to default";
            runTest("options-safe", [this, opt, def = opt.defaultValue, &errors]() -> std::pair<bool, std::string> {
                const auto [success, message] = setOption(opt.name, def);
                if (!success) errors++;
                return { success, message };
                });

            if (errors > 5) {
                Logger::testLogger().log("Too many errors occurred, stopping further setoption tests.", TraceLevel::error);
                return;
            }
        }
    }

    Logger::testLogger().logAligned("Edge case options: ",
        (errors == 0 ? "No issues encountered." : std::to_string(errors) + " failures detected. See log for details."));
}

void EngineTestController::runAnalyzeTest() {
    static constexpr auto ANALYZE_TEST_TIMEOUT = std::chrono::milliseconds(500);
    static constexpr auto LONGER_TIMEOUT = std::chrono::milliseconds(2000);

    runTest("reacts-on-stop", [this]() -> std::pair<bool, std::string> {
        auto list = EngineWorkerFactory::createEngines(engineConfig_, 1);
        TimeControl t;
        t.setInfinite();
        computeTask_->setTimeControl(t);
        for (auto fen : {
            "r3r1k1/1pq2pp1/2p2n2/1PNn4/2QN2b1/6P1/3RPP2/2R3KB b - - 0 1",
            "r1q2rk1/p2bb2p/1p1p2p1/2pPp2n/2P1PpP1/3B1P2/PP2QR1P/R1B2NK1 b - - 0 1"
            }) {
            computeTask_->newGame();
            computeTask_->setPosition(false, fen);
            computeTask_->computeMove();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            computeTask_->moveNow();
            bool finished = computeTask_->getFinishedFuture().wait_for(ANALYZE_TEST_TIMEOUT) == std::future_status::ready;
            if (!finished) {
                bool extended = computeTask_->getFinishedFuture().wait_for(LONGER_TIMEOUT) == std::future_status::ready;
                if (!extended) {
					Logger::testLogger().logAligned("Testing stop command:", "Timeout after stop command (even after extended wait)");
                    return { false, "Timeout after stop command (even after extended wait)" };
                }
            }
        }
		Logger::testLogger().logAligned("Testing stop command:", "Engine correctly handled stop command and sent bestmove");
        return { true, "" };
        });
}

void EngineTestController::runImmediateStopTest() {
    static constexpr auto ANALYZE_TEST_TIMEOUT = std::chrono::milliseconds(500);
    static constexpr auto LONGER_TIMEOUT = std::chrono::milliseconds(2000);
    runTest("correct-after-immediate-stop", [this]() -> std::pair<bool, std::string> {
        TimeControl t;
        t.setInfinite();
        computeTask_->setTimeControl(t);
		computeTask_->setPosition(false, "3r1r2/pp1q2bk/2n1nppp/2p5/3pP1P1/P2P1NNQ/1PPB3P/1R3R1K w - - 0 1");
        computeTask_->computeMove();
        computeTask_->moveNow();
        bool finished = computeTask_->getFinishedFuture().wait_for(ANALYZE_TEST_TIMEOUT) == std::future_status::ready;
        if (!finished) {
            bool extended = computeTask_->getFinishedFuture().wait_for(LONGER_TIMEOUT) == std::future_status::ready;
            if (!extended) {
                startEngine();
				Logger::testLogger().logAligned("Testing immediate stop:", "Timeout after immediate stop");
                return { false, "Timeout after immediate stop" };
            }
        }
		Logger::testLogger().logAligned("Testing immediate stop:", "Engine correctly handled immediate stop and sent bestmove");
        return { true, "" };
        });
}

void EngineTestController::runInfiniteAnalyzeTest() {
    static constexpr auto LONGER_TIMEOUT = std::chrono::milliseconds(2000);
    static constexpr auto NO_BESTMOVE_TIMEOUT = std::chrono::milliseconds(10000);

    runTest("infinite-move-does-not-exit", [this]() -> std::pair<bool, std::string> {
		std::cout << "Testing infinite mode: takes about 10 seconds, please wait...";
		std::cout.flush();
        std::cout << "\r";
        TimeControl t;
        t.setInfinite();
        computeTask_->setTimeControl(t);
		computeTask_->setPosition(false, "K7/8/k7/8/8/8/8/3r4 b - - 0 1");
        computeTask_->computeMove();
        bool exited = computeTask_->getFinishedFuture().wait_for(NO_BESTMOVE_TIMEOUT) == std::future_status::ready;
        if (exited) {
            Logger::testLogger().logAligned("Testing infinite mode:", "Engine sent bestmove without receiving 'stop'", TraceLevel::command);
            return { false, "Engine sent bestmove in infinite mode without receiving 'stop'" };
        }
        computeTask_->moveNow();
        bool stopped = computeTask_->getFinishedFuture().wait_for(LONGER_TIMEOUT) == std::future_status::ready;
        if (!stopped) {
            createGameManager();
            Logger::testLogger().logAligned("Testing infinite mode:", "Timeout after stop command", TraceLevel::command);
            return { false, "Timeout after stop command in infinite mode" };
        }
        computeTask_->getFinishedFuture().wait();
        Logger::testLogger().logAligned("Testing infinite mode:", "Correctly waited for stop and then sent bestmove", TraceLevel::command);
        return { true, "" };
        });
}

void EngineTestController::testPonderHit(const GameRecord& gameRecord, EngineWorker* engine,
    const std::string ponderMove, const std::string testname,
    std::chrono::milliseconds sleep) {
    static constexpr auto TIMEOUT = std::chrono::milliseconds(2000);

    EventSinkRecorder recorder;
    engine->setEventSink(recorder.getCallback());
    engine->newGame(gameRecord, gameRecord.isWhiteToMove());
    bool success;
    TimeControl t;
    t.addTimeSegment({ 0, 2000, 0 });
    GoLimits goLimits = createGoLimits(t, t, 0, 0, 0, true);
    engine->allowPonder(gameRecord, goLimits, ponderMove);
    std::this_thread::sleep_for(sleep);
    success = recorder.count(EngineEvent::Type::BestMove) == 0;
    checklist_->logReport(testname, success, "Engine sent a bestmove while in ponder mode. ");
    engine->setWaitForHandshake(EngineEvent::Type::BestMove);
    engine->computeMove(gameRecord, goLimits, true);
    success = engine->waitForHandshake(TIMEOUT);
    checklist_->logReport(testname, success, "Engine did not send a bestmove after compute move in ponder mode.");
}

void EngineTestController::testPonderMiss(const GameRecord& gameRecord, EngineWorker* engine,
    const std::string ponderMove, const std::string testname,
    std::chrono::milliseconds sleep) {
    static constexpr auto TIMEOUT = std::chrono::milliseconds(5000);

    EventSinkRecorder recorder;
    engine->setEventSink(recorder.getCallback());
    engine->newGame(gameRecord, gameRecord.isWhiteToMove());
    bool success;
    TimeControl t;
    t.addTimeSegment({ 0, 2000, 0 });
    GoLimits goLimits = createGoLimits(t, t, 0, 0, 0, true);
    engine->allowPonder(gameRecord, goLimits, ponderMove);
    std::this_thread::sleep_for(sleep);
    success = recorder.count(EngineEvent::Type::BestMove) == 0;
    checklist_->logReport(testname, success, "Engine sent a bestmove while in ponder mode. ");
    success = engine->moveNow(true, std::chrono::milliseconds(500));
    checklist_->logReport(testname, success, "Engine did not send a bestmove fast after receiving stop in ponder mode.");
    if (!success) {
        success = engine->waitForHandshake(TIMEOUT);
        checklist_->logReport(testname, success, "Engine never sent a bestmove after receiving stop in ponder mode.");
    }
}

void EngineTestController::runUciPonderTest() {
    const std::string testname = "correct-pondering";
    try {
        std::cout << "Testing pondering:" << std::endl;
        Timer timer;
        timer.start();
		auto& name = engineConfig_.getName();
		auto engine = computeTask_->getEngine();
        GameRecord gameRecord;
		testPonderHit(gameRecord, engine, "e2e4", testname);
        testPonderHit(gameRecord, engine, "e2e4", testname, std::chrono::milliseconds{ 0 });
		testPonderMiss(gameRecord, engine, "e2e4", testname);
        testPonderMiss(gameRecord, engine, "e2e4", testname, std::chrono::milliseconds{ 0 });
        gameRecord.setStartPosition(false, "K7/8/8/4Q3/5Q1k/8/8/8 b - - 2 68", false, name, name);
		testPonderHit(gameRecord, engine, "h4h3", testname);
        testPonderHit(gameRecord, engine, "h4h3", testname, std::chrono::milliseconds{ 0 });
        testPonderMiss(gameRecord, engine, "h4h3", testname);
        testPonderMiss(gameRecord, engine, "h4h3", testname, std::chrono::milliseconds{ 0 });
    }
	catch (const std::exception& e) {
		Logger::testLogger().log("Exception during uci ponder test: " + std::string(e.what()), TraceLevel::error);
		checklist_->logReport(testname, false, "Exception during uci ponder test: " + std::string(e.what()));
		return;
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception during uci ponder test.", TraceLevel::error);
        checklist_->logReport(testname, false, "Unknown exception during uci ponder test.");
		return;
	}
}


void EngineTestController::runEpdTests() {
	std::cout << "Testing positions, this will take a while ... \r";
	std::cout.flush();
    try {
        EngineList engines = startEngines(1);
        auto epdManager = std::make_shared<EpdTestManager>(EngineReport::getChecklist(engines[0]->getConfig().getName()));
        GameManager gameManager;
        gameManager.initUniqueEngine(std::move(engines[0]));
        gameManager.start(epdManager);
		gameManager.getFinishedFuture().wait();

        Logger::testLogger().logAligned("Testing positions:", "All positions computed.");
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during compute epd test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during compute epd test.", TraceLevel::error);
    }
}

void EngineTestController::runEpFromFenTest() {
	try {
        Logger::testLogger().logAligned("Tested ep-pos from a Fen:", "Check EP handling in FEN parsing if errors occur.");
		TimeControl timeControl;
		timeControl.addTimeSegment({ 0, 1000, 100 }); 
		computeTask_->setTimeControl(timeControl);
		computeTask_->setPosition(false, "rnbqkb1r/ppp2ppp/8/3pP3/4n3/5N2/PPP2PPP/RNBQKB1R w KQkq d6 0 1",
            std::vector<std::string>{ "e5d6" });
		computeTask_->computeMove();
        computeTask_->getFinishedFuture().wait_for(std::chrono::seconds(2));
	}
	catch (const std::exception& e) {
		Logger::testLogger().log("Exception during compute ep-pos test: " + std::string(e.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception during compute ep-pos test.", TraceLevel::error);
	}

}

void EngineTestController::runComputeGameTest() {
	Logger::testLogger().log("\nThe engine now plays against itself. I control all engine output, and check its validity while playing.");
    EngineList engines = startEngines(2);
	computeTask_->initEngines(std::move(engines));
    try {
        computeTask_->newGame();
        computeTask_->setPosition(true);
        TimeControl t1; t1.addTimeSegment({ 0, 20000, 100 });
        TimeControl t2; t2.addTimeSegment({ 0, 10000, 100 });
        computeTask_->setTimeControls({ t1, t2 });
        computeTask_->autoPlay(true);
        computeTask_->getFinishedFuture().wait();
    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during compute games test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during compute games test.", TraceLevel::error);
    }
}

void EngineTestController::runPonderGameTest() {
    Logger::testLogger().log("\nThe engine now plays against itself with pondering enabled. ");
    EngineList engines = startEngines(2);
	engines[0]->getConfigMutable().setPonder(true);
	engines[1]->getConfigMutable().setPonder(true);
    computeTask_->initEngines(std::move(engines));
    try {
        computeTask_->newGame();
        computeTask_->setPosition(true);
        TimeControl t1; t1.addTimeSegment({ 0, 20000, 100 });
        TimeControl t2; t2.addTimeSegment({ 0, 10000, 100 });
        computeTask_->setTimeControls({ t1, t2 });
        computeTask_->autoPlay(true);
        computeTask_->getFinishedFuture().wait();
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during compute games test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during compute games test.", TraceLevel::error);
    }
}

void EngineTestController::runMultipleGamesTest() {
    uint32_t parallelGames = CliSettings::Manager::get<uint32_t>("concurrency");

    Logger::testLogger().log("\nTesting playing games. The engine will play " + std::to_string(numGames_) + 
        " games in total, " + std::to_string(parallelGames) + " in parallel.");
	Logger::testLogger().log("You can alter the number of games played with 'numgames' option and the number of parallel games with --concurrency option. ");
    Logger::testLogger().log("White has always the longer time control so we expect white to win most games. ");
    Logger::testLogger().log("Please wait a moment before first game results occur.");

	GameManagerPool::getInstance().setConcurrency(parallelGames, true);
    auto tournament = std::make_shared<TestTournament>(numGames_, checklist_);

    try {
        GameManagerPool::getInstance().addTaskProvider(tournament, engineConfig_, engineConfig_);
		GameManagerPool::getInstance().assignTaskToManagers();
        GameManagerPool::getInstance().waitForTask();
        Logger::testLogger().log("All games completed.");
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during compute games test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during compute games test.", TraceLevel::error);
    }
}

void EngineTestController::runPlaceholderTest() {
    try {
        // No-op test for demonstration
    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during placeholder test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during placeholder test.", TraceLevel::error);
    }
}
