/**
 * @file movescanner_test.cpp
 * @brief Unit tests for the MoveScanner class
 */

#include "qapla-engine/movescanner.h"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace QaplaInterface {
namespace Test {

struct MoveScannerTestCase {
    std::string input;
    bool expectedLegal;
    std::optional<char> expectedPiece = std::nullopt;
    std::optional<char> expectedPromote = std::nullopt;
    std::optional<int32_t> expectedDepartureFile = std::nullopt;
    std::optional<int32_t> expectedDepartureRank = std::nullopt;
    std::optional<int32_t> expectedDestinationFile = std::nullopt;
    std::optional<int32_t> expectedDestinationRank = std::nullopt;
    std::optional<bool> expectedIsLan = std::nullopt;
};

const std::vector<MoveScannerTestCase> testCases = {
    // Invalid inputs
    { .input = "[",     .expectedLegal = false },
    { .input = "i4",    .expectedLegal = false },
    { .input = "hello", .expectedLegal = false },
    { .input = "",      .expectedLegal = false },
    { .input = "9",     .expectedLegal = false },
    { .input = "@#$",   .expectedLegal = false },
    
    // Valid pawn move "e4"
    { 
        .input = "e4", 
        .expectedLegal = true,
        .expectedPiece = 'P',
        .expectedDestinationFile = 4,
        .expectedDestinationRank = 3
    },
    
    // Valid knight move "Nf3"
    { 
        .input = "Nf3", 
        .expectedLegal = true,
        .expectedPiece = 'N',
        .expectedDestinationFile = 5,
        .expectedDestinationRank = 2
    },
    
    // Valid short castle "O-O"
    { 
        .input = "O-O", 
        .expectedLegal = true,
        .expectedPiece = 'K',
        .expectedDepartureFile = 4,
        .expectedDestinationFile = 6
    },
    
    // Valid long castle "O-O-O"
    { 
        .input = "O-O-O", 
        .expectedLegal = true,
        .expectedPiece = 'K',
        .expectedDepartureFile = 4,
        .expectedDestinationFile = 2
    },
    
    // Valid promotion "e8=Q"
    { 
        .input = "e8=Q", 
        .expectedLegal = true,
        .expectedPiece = 'P',
        .expectedPromote = 'Q',
        .expectedDestinationFile = 4,
        .expectedDestinationRank = 7
    },
    
    // Valid capture "Nxe5"
    { 
        .input = "Nxe5", 
        .expectedLegal = true,
        .expectedPiece = 'N',
        .expectedDestinationFile = 4,
        .expectedDestinationRank = 4
    },
    
    // Valid LAN move "e2e4" - note: piece is 0 (not 'P') for LAN moves without explicit piece
    { 
        .input = "e2e4", 
        .expectedLegal = true,
        .expectedDepartureFile = 4,
        .expectedDepartureRank = 1,
        .expectedDestinationFile = 4,
        .expectedDestinationRank = 3,
        .expectedIsLan = true
    },

    // Valid disambiguation by file "Rae1"
    {
        .input = "Rae1",
        .expectedLegal = true,
        .expectedPiece = 'R',
        .expectedDepartureFile = 0,
        .expectedDestinationFile = 4,
        .expectedDestinationRank = 0
    },

    // Valid disambiguation by rank "R1e4"
    {
        .input = "R1e4",
        .expectedLegal = true,
        .expectedPiece = 'R',
        .expectedDepartureRank = 0,
        .expectedDestinationFile = 4,
        .expectedDestinationRank = 3
    },

    // Valid pawn capture "exd5"
    {
        .input = "exd5",
        .expectedLegal = true,
        .expectedPiece = 'P',
        .expectedDepartureFile = 4,
        .expectedDestinationFile = 3,
        .expectedDestinationRank = 4
    },

    // Valid move with check "Qh7+"
    {
        .input = "Qh7+",
        .expectedLegal = true,
        .expectedPiece = 'Q',
        .expectedDestinationFile = 7,
        .expectedDestinationRank = 6
    },

    // Valid move with mate "Qh7#"
    {
        .input = "Qh7#",
        .expectedLegal = true,
        .expectedPiece = 'Q',
        .expectedDestinationFile = 7,
        .expectedDestinationRank = 6
    }
};

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

class MoveScannerTest {
public:
    std::vector<TestResult> results;

    void runAllTests() {
        for (const auto& testCase : testCases) {
            runTest(testCase);
        }
    }

private:
    void runTest(const MoveScannerTestCase& testCase) {
        MoveScanner scanner(testCase.input);
        TestResult result;
        result.name = "Input: \"" + testCase.input + "\"";
        result.passed = true;
        
        // Check legal status
        if (scanner.isLegal() != testCase.expectedLegal) {
            result.passed = false;
            result.message = "Expected legal=" + std::string(testCase.expectedLegal ? "true" : "false") +
                           ", got legal=" + std::string(scanner.isLegal() ? "true" : "false");
            results.push_back(result);
            return;
        }

        // If expected to be illegal, no further checks needed
        if (!testCase.expectedLegal) {
            result.message = "Correctly identified as illegal";
            results.push_back(result);
            return;
        }

        // Check optional fields
        std::vector<std::string> errors;

        if (testCase.expectedPiece.has_value() && 
            scanner.piece != testCase.expectedPiece.value()) {
            errors.push_back("piece: expected '" + std::string(1, testCase.expectedPiece.value()) + 
                           "', got '" + std::string(1, scanner.piece) + "'");
        }

        if (testCase.expectedPromote.has_value() && 
            scanner.promote != testCase.expectedPromote.value()) {
            errors.push_back("promote: expected '" + std::string(1, testCase.expectedPromote.value()) + 
                           "', got '" + std::string(1, scanner.promote) + "'");
        }

        if (testCase.expectedDepartureFile.has_value() && 
            scanner.departureFile != testCase.expectedDepartureFile.value()) {
            errors.push_back("departureFile: expected " + std::to_string(testCase.expectedDepartureFile.value()) + 
                           ", got " + std::to_string(scanner.departureFile));
        }

        if (testCase.expectedDepartureRank.has_value() && 
            scanner.departureRank != testCase.expectedDepartureRank.value()) {
            errors.push_back("departureRank: expected " + std::to_string(testCase.expectedDepartureRank.value()) + 
                           ", got " + std::to_string(scanner.departureRank));
        }

        if (testCase.expectedDestinationFile.has_value() && 
            scanner.destinationFile != testCase.expectedDestinationFile.value()) {
            errors.push_back("destinationFile: expected " + std::to_string(testCase.expectedDestinationFile.value()) + 
                           ", got " + std::to_string(scanner.destinationFile));
        }

        if (testCase.expectedDestinationRank.has_value() && 
            scanner.destinationRank != testCase.expectedDestinationRank.value()) {
            errors.push_back("destinationRank: expected " + std::to_string(testCase.expectedDestinationRank.value()) + 
                           ", got " + std::to_string(scanner.destinationRank));
        }

        if (testCase.expectedIsLan.has_value() && 
            scanner.isLan() != testCase.expectedIsLan.value()) {
            errors.push_back("isLan: expected " + std::string(testCase.expectedIsLan.value() ? "true" : "false") + 
                           ", got " + std::string(scanner.isLan() ? "true" : "false"));
        }

        if (!errors.empty()) {
            result.passed = false;
            result.message = "";
            for (size_t i = 0; i < errors.size(); ++i) {
                if (i > 0) result.message += "; ";
                result.message += errors[i];
            }
        } else {
            result.message = "Correctly parsed";
        }

        results.push_back(result);
    }

public:
    void printResults() const {
        std::cout << "\n========================================\n";
        std::cout << "MoveScanner Unit Test Results\n";
        std::cout << "========================================\n\n";
        
        int passed = 0;
        int failed = 0;
        
        for (const auto& result : results) {
            std::cout << (result.passed ? "[PASS] " : "[FAIL] ");
            std::cout << result.name << "\n";
            std::cout << "       " << result.message << "\n\n";
            
            if (result.passed) {
                ++passed;
            } else {
                ++failed;
            }
        }
        
        std::cout << "========================================\n";
        std::cout << "Summary: " << passed << " passed, " << failed << " failed\n";
        std::cout << "========================================\n";
    }

    [[nodiscard]] bool allPassed() const {
        for (const auto& result : results) {
            if (!result.passed) {
                return false;
            }
        }
        return true;
    }
};

} // namespace Test
} // namespace QaplaInterface

// Function to run all MoveScanner tests, called from main
bool runMoveScannerTests() {
    QaplaInterface::Test::MoveScannerTest test;
    test.runAllTests();
    test.printResults();
    return test.allPassed();
}
