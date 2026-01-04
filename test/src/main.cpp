/**
 * @file main.cpp
 * @brief Main entry point for qapla-engine-tester unit tests
 */

#include <iostream>
#include <vector>
#include <string>
#include <functional>

// Forward declarations of test runners
bool runMoveScannerTests();

struct TestSuite {
    std::string name;
    std::function<bool()> runner;
};

int main() {
    std::cout << "==============================================\n";
    std::cout << "Qapla Engine Tester - Unit Tests\n";
    std::cout << "==============================================\n\n";

    // Register all test suites
    std::vector<TestSuite> testSuites = {
        {"MoveScanner Tests", runMoveScannerTests}
    };

    int totalPassed = 0;
    int totalFailed = 0;

    // Run all test suites
    for (const auto& suite : testSuites) {
        std::cout << "Running: " << suite.name << "\n";
        std::cout << "----------------------------------------------\n";
        
        bool passed = suite.runner();
        
        if (passed) {
            ++totalPassed;
        } else {
            ++totalFailed;
        }
        
        std::cout << "\n";
    }

    // Print final summary
    std::cout << "==============================================\n";
    std::cout << "Final Summary\n";
    std::cout << "==============================================\n";
    std::cout << "Test Suites Passed: " << totalPassed << "\n";
    std::cout << "Test Suites Failed: " << totalFailed << "\n";
    std::cout << "==============================================\n";

    return totalFailed > 0 ? 1 : 0;
}
