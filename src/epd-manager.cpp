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

#include "epd-manager.h"
#include "engine-worker-factory.h"
#include "game-manager.h"
#include "game-manager-pool.h"
#include "game-state.h"
#include "string-helper.h"

namespace QaplaTester {

std::string EpdManager::generateHeaderLine() const {
    std::ostringstream header;
    auto formatEngineName = [](const std::string& name) -> std::string {
        constexpr size_t totalWidth = 25;
        size_t len = name.length();
        if (len > totalWidth) {
            return "..." + name.substr(len - (totalWidth - 3));
        }
        size_t padding = totalWidth - static_cast<uint32_t>(name.length());
        size_t leftPad = padding / 2;
        size_t rightPad = padding - leftPad;
        return std::string(leftPad, ' ') + name + std::string(rightPad, ' ');
        };
    header << std::setw(20) << std::left << "TestId";
    auto results = getResultsCopy();
    for (const auto& result : results) {
        if (result.testSetName == epdFileName_) {
            header << "|" << formatEngineName(result.engineName);
        }
    }
    
    return header.str();
}

void EpdManager::logHeaderLine() const {
    auto header = generateHeaderLine();
    Logger::testLogger().log(header, TraceLevel::result);
}

static void formatInlineResult(std::ostream& os, const EpdTestCase& test) {
    os << "|" << std::setw(8) << std::right << (test.correct ? QaplaHelpers::formatMs(test.correctAtTimeInMs, 2) : "-")
        << ", D:" << std::setw(3) << std::right << (test.correct ? std::to_string(test.correctAtDepth) : "-")
        << ", M: " << std::setw(5) << std::left << test.playedMove;
}

std::string EpdManager::generateResultLine(const EpdTestCase& current, const TestResults& results) {
    std::ostringstream line;
    line << std::setw(20) << std::left << current.id;
    for (const auto& result : results) {
        const auto it = std::ranges::find_if(result.result, [&](const EpdTestCase& t) {
            return t.id == current.id;
            });
        if (it != result.result.end()) {
            formatInlineResult(line, *it);
        }
        else {
            line << "|" << std::setw(23) << "?";
        }
    }

    line << "| BM: ";
    for (const auto& bm : current.bestMoves) {
        line << bm << " ";
    }
    return line.str();
}

void EpdManager::logResultLine(const EpdTestCase& current) const {
	auto results = getResultsCopy();
    auto line = generateResultLine(current, results);
	Logger::testLogger().log(line, TraceLevel::result);
}

void EpdManager::saveResults(std::ostream& os) const {
    auto results = getResultsCopy();
    if (results.empty()) {
        return;
    }
    os << generateHeaderLine() << "\n" << std::flush;
    for (const auto& testCase : testsRead_) {
        os << generateResultLine(testCase, results) << "\n" << std::flush;
    }
}

static uint64_t timeColumnToMs(const std::string& timeStr) {
    // format HH:MM:SS.msc (HH optional, MM optional)
    std::istringstream iss(timeStr);
    std::string part;
    uint64_t totalMs = 0;
    std::vector<std::string> parts;
    while (std::getline(iss, part, ':')) {
        parts.push_back(part);
    }
    if (parts.size() > 3) {
        throw std::runtime_error("Invalid time format: " + timeStr);
    }
    size_t start = 0;
    if (parts.size() == 3) {
        auto hours = QaplaHelpers::to_int(parts[0]).value_or(0);
        if (hours >= 0) {
            totalMs += static_cast<uint64_t>(hours) * 3600000;
        }
        start = 1;
    }
    if (parts.size() - start == 2) {
        auto minutes = QaplaHelpers::to_int(parts[start]).value_or(0);
        if (minutes >= 0) {
            totalMs += static_cast<uint64_t>(minutes) * 60000;
        }
        start += 1;
    }
    if (parts.size() - start == 1) {
        const auto& secPart = parts[start];
        size_t dotPos = secPart.find('.');
        auto seconds = 0;
        if (dotPos != std::string::npos) {
            seconds = QaplaHelpers::to_int(secPart.substr(0, dotPos)).value_or(0);
            auto millis = QaplaHelpers::to_int(secPart.substr(dotPos + 1)).value_or(0);
            auto fracDigits = secPart.length() - dotPos - 1;
            if (millis >= 0 && fracDigits <= 3) {
                for (size_t i = fracDigits; i < 3; ++i) {
                    millis *= 10;
                }
                totalMs += static_cast<uint64_t>(millis);
            }
        }
        else {
            seconds = QaplaHelpers::to_int(secPart).value_or(0);
        }
        if (seconds >= 0) {
            totalMs += static_cast<uint64_t>(seconds) * 1000;
        }
    }
    return totalMs;
}

static int depthColumnToInt(const std::string& depthStr) {
    // Format D: <number>
    auto pos = depthStr.find("D:");
    if (pos != std::string::npos) {
        auto valueStr = QaplaHelpers::trim(depthStr.substr(pos + 2));
        return QaplaHelpers::to_int(valueStr).value_or(-1);
    }
    return -1;
}

static std::string moveColumnToStr(const std::string& moveCol) {
    // Format M: <move>
    auto pos = moveCol.find("M:");
    if (pos != std::string::npos) {
        return QaplaHelpers::trim(moveCol.substr(pos + 2));
    }
    return "-";
}

std::vector<std::string> parseResultLine(const std::string& line) {
    std::vector<std::string> columns;
    std::istringstream iss(line);
    std::string column;
    while (std::getline(iss, column, '|')) {
        columns.push_back(QaplaHelpers::trim(column));
    }
    return columns;
}

std::vector<std::string> parseEngineResult(const std::string& column) {
    std::vector<std::string> result;
    std::istringstream iss(column);
    std::string token;
    while (std::getline(iss, token, ',')) {
        result.push_back(QaplaHelpers::trim(token));
    }
    return result;
}

static std::vector<EpdTestResult> 
loadTestResults(std::istream& is, const TimeControl& tc, std::vector<EpdTestCase>& testsRead) {
    std::vector<EpdTestResult> results;
    std::string headerLine;
    if (!std::getline(is, headerLine)) {
        return results;
    }
    auto headers = parseResultLine(headerLine);
    if (headers.size() < 2 || headers[0] != "TestId") {
        return results;
    }
    for (size_t i = 1; i < headers.size(); ++i) {
        results.push_back(EpdTestResult{});
        results.back().engineName = headers[i];
        results.back().tc_ = tc; 
    }

    std::string line;
    while (std::getline(is, line)) {
        auto columns = parseResultLine(line);
        if (columns.size() < 3) {
            continue;
        }
        if (columns.back().find("BM:") == std::string::npos) {
            continue; // No BM field, invalid line
        }
        const auto& testId = columns[0];
        auto it = std::ranges::find_if(testsRead, [&](const EpdTestCase& t) {
            return t.id == testId;
            });
        if (it == testsRead.end()) {
            continue; // No matching test case found, skip
        }
        const auto columnsWithoutBm = columns.size() - 1;
        for (size_t i = 1; i < columnsWithoutBm && i - 1 < results.size(); ++i) {
            auto engineResults = parseEngineResult(columns[i]);
            EpdTestCase testCase = *it; // Copy original test case
            testCase.id = testId;
            if (engineResults.size() >= 3) {
                testCase.tested = true;
                testCase.correctAtTimeInMs = engineResults[0] != "-" ? timeColumnToMs(engineResults[0]) : 0;
                testCase.correctAtDepth = engineResults[1] != "-" ? depthColumnToInt(engineResults[1]) : -1;
                testCase.playedMove = moveColumnToStr(engineResults[2]);
                testCase.correct = testCase.correctAtDepth != -1; // If depth is known, we assume correctness
            }
            results[i - 1].result.push_back(testCase);
        }
    }

    return results;
}

bool EpdManager::loadResults(std::istream& is) {
    auto testResults = loadTestResults(is, tc_, testsRead_);
    if (testResults.empty()) {
        return false;
    }
    for (auto& test : testResults) {
        test.testSetName = epdFileName_;
        // Find existing instance or create new one
        auto it = std::ranges::find_if(testInstances_, [&](const std::shared_ptr<EpdTest>& instance) {
            auto results = instance->getResultsCopy();
            return results.engineName == test.engineName;
            });
        if (it != testInstances_.end()) {
            // Update existing instance
            (*it)->initialize(test);
        }
        else {
            // Create new instance
            auto newTest = std::make_shared<EpdTest>();
            newTest->initialize(test);
            testInstances_.emplace_back(newTest);
        }
    }
    return true;
}

TestResults EpdManager::getResultsCopy() const {
		TestResults results;
        for (const auto& instance : testInstances_) {
            results.push_back(instance->getResultsCopy());
		}
        return results;
	}


void EpdManager::initializeTestCases(uint64_t maxTimeInS, uint64_t minTimeInS, uint32_t seenPlies) {
    if (!reader_) {
        throw std::runtime_error("EpdReader must be initialized before loading test cases.");
    }

    (*reader_).reset();
    testInstances_.clear();

    while (true) {
        auto testCase = nextTestCaseFromReader();
        if (!testCase) {
            break;
        }
		testCase->maxTimeInS = maxTimeInS;
		testCase->minTimeInS = minTimeInS;
		testCase->seenPlies = static_cast<int>(seenPlies);
        testsRead_.push_back(std::move(*testCase));
    }
}

void EpdManager::initialize(const std::string& filepath, 
    uint64_t maxTimeInS, uint64_t minTimeInS, uint32_t seenPlies)
{
	epdFileName_ = filepath;
    bool sameFile = reader_ && reader_->getFilePath() == filepath;
    if (!sameFile) {
        reader_ = std::make_unique<EpdReader>(filepath);
    }

    initializeTestCases(maxTimeInS, minTimeInS, seenPlies);
    tc_.setMoveTime(maxTimeInS * 1000);
}

void EpdManager::clear() {
    if (reader_) {
        (*reader_).reset();
    }
    testInstances_.clear();
    testsRead_.clear();
}

void EpdManager::schedule(const EngineConfig& engineConfig, GameManagerPool& pool) {
    EpdTestResult test;
	test.tc_ = tc_;
    test.engineName = engineConfig.getName();
    test.result = testsRead_;
	test.testSetName = epdFileName_;
	auto newTest = std::make_shared<EpdTest>();
	newTest->initialize(test);
	testInstances_.emplace_back(newTest);
    newTest->setTestResultCallback([this]([[maybe_unused]] EpdTest* test, size_t first, size_t last) {
        auto results = getResultsCopy();
        for (size_t i = first; i < last && i < testsRead_.size(); ++i) {
            const auto& current = testsRead_[i];
            auto line = generateResultLine(current, results);
            Logger::testLogger().log(line, TraceLevel::result);
        }
    });
    logHeaderLine();
    EpdTest::schedule(newTest, engineConfig, pool);
}

void EpdManager::continueAnalysis() {
    for (const auto& instance : testInstances_) {
        instance->continueAnalysis();
    }
}

std::optional<EpdTestCase> EpdManager::nextTestCaseFromReader() {
    if (!reader_) {
        return std::nullopt;
    }

    auto entryOpt = reader_->next();
    if (!entryOpt) {
        return std::nullopt;
    }

    const auto& entry = *entryOpt;
    EpdTestCase testCase;
    testCase.fen = entry.fen;
    testCase.original = entry;

    auto it = entry.operations.find("id");
    if (it != entry.operations.end() && !it->second.empty()) {
        testCase.id = it->second.front();
    }

    auto bmIt = entry.operations.find("bm");
    if (bmIt != entry.operations.end()) {
        testCase.bestMoves = bmIt->second;
    }

    return testCase;
}

double EpdManager::getSuccessRate() const {
	int totalTests = 0;
	int correctTests = 0;
	for (const auto& instance : testInstances_) {
		for (const auto& test : instance->getResultsCopy().result) {
			++totalTests;
			if (test.correct) {
				++correctTests;
			}
		}
	}
	return totalTests > 0 ? static_cast<double>(correctTests) / totalTests : 0.0;
}

} // namespace QaplaTester
