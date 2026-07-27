/**
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../../base-elements/time-control.h"
#include "../../base-elements/ini-file.h"

using namespace QaplaTester;
using Catch::Approx;

TEST_CASE("Time Control Parsing", "[unit][time-control]") {
    SECTION("Standard Time Control (Seconds)") {
        auto tc = TimeControl::parse("60");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 60000);
        REQUIRE(tc.timeSegments()[0].incrementMs == 0);
        REQUIRE(tc.timeSegments()[0].movesToPlay == 0); // Sudden death
        REQUIRE(tc.toPgnTimeControlString() == "60.0");
    }

    SECTION("Time Control with Increment") {
        auto tc = TimeControl::parse("60+1");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 60000);
        REQUIRE(tc.timeSegments()[0].incrementMs == 1000);
        REQUIRE(tc.toPgnTimeControlString() == "60.0+1.00");
    }

    SECTION("Time Control with Moves") {
        auto tc = TimeControl::parse("40/300");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 300000);
        REQUIRE(tc.timeSegments()[0].movesToPlay == 40);
        REQUIRE(tc.toPgnTimeControlString() == "40/300.0");
    }

    SECTION("Time Control with Moves and Increment") {
        auto tc = TimeControl::parse("40/300+2");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 300000);
        REQUIRE(tc.timeSegments()[0].incrementMs == 2000);
        REQUIRE(tc.timeSegments()[0].movesToPlay == 40);
        REQUIRE(tc.toPgnTimeControlString() == "40/300.0+2.00");
    }

    SECTION("Multiple Segments") {
        auto tc = TimeControl::parse("40/7200:3600");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 2);
        
        REQUIRE(tc.timeSegments()[0].movesToPlay == 40);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 7200000);
        
        REQUIRE(tc.timeSegments()[1].movesToPlay == 0);
        REQUIRE(tc.timeSegments()[1].baseTimeMs == 3600000);
        
        REQUIRE(tc.toPgnTimeControlString() == "40/7200.0:3600.0");
    }

    SECTION("H:M:S Format Support") {
        auto tc = TimeControl::parse("1:30");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 90000);
    }
    
    SECTION("H:M:S with increment") {
        auto tc = TimeControl::parse("1:00+5");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 60000);
        REQUIRE(tc.timeSegments()[0].incrementMs == 5000);
    }
    
    SECTION("Decimal Times") {
        auto tc = TimeControl::parse("0.5+0.1");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 500);
        REQUIRE(tc.timeSegments()[0].incrementMs == 100);
    }

    SECTION("Parsing edge case 0/0:01+0.1") {
        // This validates the fix for the reported bug
        auto tc = TimeControl::parse("0/0:01+0.1");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        REQUIRE(tc.timeSegments()[0].movesToPlay == 0);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 1000); // 1 sec
        REQUIRE(tc.timeSegments()[0].incrementMs == 100);  // 0.1 sec
    }
}

TEST_CASE("Special Time Controls", "[unit][time-control]") {
    SECTION("Infinite") {
        auto tc = TimeControl::parse("inf");
        REQUIRE(tc.isValid());
        REQUIRE(tc.infinite());
        REQUIRE(tc.toPgnTimeControlString() == "inf");
    }

    SECTION("Fixed Depth") {
        auto tc = TimeControl::parse("depth:10");
        REQUIRE(tc.isValid());
        REQUIRE(tc.depth().has_value());
        REQUIRE(tc.depth().value() == 10);
        REQUIRE(tc.toPgnTimeControlString() == "depth: 10");
    }

    SECTION("Fixed Nodes") {
        auto tc = TimeControl::parse("nodes:1000");
        REQUIRE(tc.isValid());
        REQUIRE(tc.nodes().has_value());
        REQUIRE(tc.nodes().value() == 1000);
        REQUIRE(tc.toPgnTimeControlString() == "nodes: 1000");
    }

    SECTION("Fixed Movetime") {
        auto tc = TimeControl::parse("movetime(ms):500");
        REQUIRE(tc.isValid());
        REQUIRE(tc.moveTimeMs().has_value());
        REQUIRE(tc.moveTimeMs().value() == 500);
        REQUIRE(tc.toPgnTimeControlString() == "movetime(ms): 500");
    }
    
    SECTION("Mate Search") {
        auto tc = TimeControl::parse("mate:5");
        REQUIRE(tc.isValid());
        REQUIRE(tc.mateIn().has_value());
        REQUIRE(tc.mateIn().value() == 5);
        REQUIRE(tc.toPgnTimeControlString() == "mate: 5");
    }
}

TEST_CASE("GoLimits Calculation", "[unit][time-control]") {
    SECTION("Simple Sudden Death") {
        // 60s + 1s, sudden death
        auto tc = TimeControl::parse("60+1");
        // Move 1 (White), 0 moves played so far (start of game)
        // timeUsed = 0.
        auto limits = createGoLimits(tc, tc, 0, 0, 0, true);
        
        REQUIRE(limits.hasTimeControl);
        REQUIRE(limits.wtimeMs == 60000);
        REQUIRE(limits.wincMs == 1000);
        REQUIRE(limits.movesToGo == 0);
        
        // Move 2 (White), after 1 move played. 
        // 5s used.
        // halfMoves = 2 (White moved, Black moved) -> wMovesPlayed=1.
        limits = createGoLimits(tc, tc, 2, 5000, 5000, true);
        
        // Time = 60000 + 1*1000 (inc for move 1) - 5000 (used) = 56000
        REQUIRE(limits.wtimeMs == 56000);
        REQUIRE(limits.wincMs == 1000);
    }
    
    SECTION("Sudden Death with moves to play 0 but not infinite") {
        // verify movesToGo is 0
        auto tc = TimeControl::parse("0/60+1");
        auto limits = createGoLimits(tc, tc, 0, 0, 0, true);
        REQUIRE(limits.movesToGo == 0);
    }
    
    SECTION("Fixed Moves Segment") {
        // 40/60000+0
        auto tc = TimeControl::parse("40/60");
        // Start
        auto limits = createGoLimits(tc, tc, 0, 0, 0, true);
        REQUIRE(limits.wtimeMs == 60000);
        REQUIRE(limits.movesToGo == 40);
        
        // After 10 moves (20 half moves), 10s used
        limits = createGoLimits(tc, tc, 20, 10000, 10000, true);
        // Time = 60000 - 10000 = 50000
        // MovesToGo = 40 - 10 = 30
        REQUIRE(limits.wtimeMs == 50000);
        REQUIRE(limits.movesToGo == 30);
    }

    SECTION("Time Running Out") {
         auto tc = TimeControl::parse("60+0");
         // Used 61s
         auto limits = createGoLimits(tc, tc, 10, 61000, 0, true);
         REQUIRE(limits.wtimeMs == 0);
    }
}

TEST_CASE("INI Conversion", "[unit][time-control]") {
    auto tc = TimeControl::parse("40/300+1");
    auto section = tc.toSection("standard");
    
    REQUIRE(section.name == "timecontrol");
    REQUIRE(section.getValue("name").has_value());
    REQUIRE(section.getValue("name").value() == "standard");
    REQUIRE(section.getValue("tc").has_value());
    REQUIRE(section.getValue("tc").value() == "40/300.0+1.00");
    
    TimeControl tc2;
    tc2.fromSection(section);
    REQUIRE(tc == tc2);
}

TEST_CASE("Detailed Logic Check 3+0.02", "[unit][time-control][debug]") {
    // User Scenario: 3+0.02
    std::string tcString = "3+0.02";
    auto tc = TimeControl::parse(tcString);

    SECTION("Parsing Correctness") {
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        
        const auto& seg = tc.timeSegments()[0];
        // 3s = 3000ms
        REQUIRE(seg.baseTimeMs == 3000); 
        // 0.02s = 20ms
        REQUIRE(seg.incrementMs == 20);
        // Sudden death
        REQUIRE(seg.movesToPlay == 0);
    }

    SECTION("Calculation Logic - GoLimits") {
        // Initial state: Start of game (Move 1 White)
        // halfMoves=0, timeUsed=0
        auto limits = createGoLimits(tc, tc, 0, 0, 0, true);
        
        REQUIRE(limits.wtimeMs == 3000);
        REQUIRE(limits.wincMs == 20);
        REQUIRE(limits.movesToGo == 0);

        // Move 2 White (After Black moved). 
        // White played 1 move. 
        // Suppose White used 1000ms.
        // Formula: Base + MovesPlayed*Inc - TimeUsed
        // 3000 + 1*20 - 1000 = 2020
        limits = createGoLimits(tc, tc, 2, 1000, 1000, true);
        REQUIRE(limits.wtimeMs == 2020);
        REQUIRE(limits.wincMs == 20);
    }
}

TEST_CASE("Recalculate 0.5 sec scenario", "[unit][time-control][debug]") {
    auto tc = TimeControl::parse("0.5"); // 500ms + 0inc
    
    SECTION("Initial") {
        auto limits = createGoLimits(tc, tc, 0, 0, 0, true);
        REQUIRE(limits.wtimeMs == 500);
    }
    
    SECTION("Step 2 - Used 17ms") {
        // Move 1 White. Used 17ms. Next is Move 2 Black.
        // Then Move 3 White.
        // User said: "Engine gets 500ms, consumes 17ms, then gets 196ms".
        // This implies for the NEXT move of the same engine.
        // So White Move 2.
        // halfMoves = 2. wMovesPlayed = 1.
        // Time Used = 17ms.
        // TimeLeft = 500 + 1*0 - 17 = 483ms.
        auto limits = createGoLimits(tc, tc, 2, 17, 0, true);
        REQUIRE(limits.wtimeMs == 483);
    }
}

TEST_CASE("createGoLimits Regression", "[unit][time-control][go-limits]") {
    SECTION("Sub-second Time Control") {
        auto tc = TimeControl::parse("0.5");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments().size() == 1);
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 500);

        auto limits = createGoLimits(tc, tc, 0, 0, 0, true);
        REQUIRE(limits.hasTimeControl);
        REQUIRE(limits.wtimeMs == 500);
        REQUIRE(limits.btimeMs == 500);
    }

    SECTION("Sub-second with Increment") {
        auto tc = TimeControl::parse("0.5+0.1");
        REQUIRE(tc.isValid());
        REQUIRE(tc.timeSegments()[0].baseTimeMs == 500);
        REQUIRE(tc.timeSegments()[0].incrementMs == 100);

        auto limits = createGoLimits(tc, tc, 0, 0, 0, true);
        REQUIRE(limits.wtimeMs == 500);
        REQUIRE(limits.wincMs == 100);
    }

    SECTION("Time Used > Base Time resets to 0") {
        auto tc = TimeControl::parse("0.5");
        // 600ms used vs 500ms base
        auto limits = createGoLimits(tc, tc, 0, 600, 0, true);
        REQUIRE(limits.wtimeMs == 0);
    }
}

TEST_CASE("GoLimits with black to move at start", "[unit][time-control][go-limits]") {
    // Games started from an opening/EPD position may begin with black to move.
    // The per-side move counts must then be assigned by start side, not by
    // the plain half-move parity.

    SECTION("Increment goes to the side that actually moved") {
        auto tc = TimeControl::parse("60+1");
        // Black moved first (halfMoves = 1), white to move next.
        // Black played 1 move and used 2500ms, white played none.
        auto limits = createGoLimits(tc, tc, 1, 0, 2500, true);

        // White: 60000 + 0*1000 - 0
        REQUIRE(limits.wtimeMs == 60000);
        // Black: 60000 + 1*1000 - 2500
        REQUIRE(limits.btimeMs == 58500);
    }

    SECTION("No increment: only the mover loses time") {
        auto tc = TimeControl::parse("60");
        auto limits = createGoLimits(tc, tc, 1, 0, 2525, true);

        REQUIRE(limits.wtimeMs == 60000);
        REQUIRE(limits.btimeMs == 57475);
    }

    SECTION("movesToGo counts per side") {
        auto tc = TimeControl::parse("40/60");
        // Black started; after 3 half moves (b, w, b) white is to move.
        // White played 1 move -> 39 to go.
        auto limits = createGoLimits(tc, tc, 3, 1000, 2000, true);
        REQUIRE(limits.movesToGo == 39);

        // After 2 half moves (b, w) black is to move again.
        // Black played 1 move -> 39 to go.
        limits = createGoLimits(tc, tc, 2, 1000, 1000, false);
        REQUIRE(limits.movesToGo == 39);
    }
}
