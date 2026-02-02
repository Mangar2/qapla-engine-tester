#!/usr/bin/env python3
"""Integration Test Runner for Qapla Engine Tester

Runs integration tests and reports results.
"""

import argparse
import importlib.util
import sys
from pathlib import Path
from typing import Dict, Any, List

from test_framework import Colors, invoke_test


RESULTS_FILE = Path("test/integration/test_results.log")


def load_previous_results() -> Dict[str, str]:
    """Load results from previous runs."""
    results = {}
    if not RESULTS_FILE.exists():
        return results

    try:
        with open(RESULTS_FILE, "r", encoding="utf-8") as f:
            for line in f:
                # Split by the last ' - ' to handle test names containing ' - '
                parts = line.strip().rsplit(" - ", 1)
                if len(parts) == 2:
                    results[parts[0]] = parts[1]
    except Exception as e:
        print(f"{Colors.YELLOW}Warning: Could not read results file: {e}{Colors.RESET}")
    return results


def save_results(results: Dict[str, str]):
    """Save all cumulative results to the protocol file."""
    try:
        with open(RESULTS_FILE, "w", encoding="utf-8") as f:
            for name in sorted(results.keys()):
                f.write(f"{name} - {results[name]}\n")
    except Exception as e:
        print(f"{Colors.RED}Error saving results file: {e}{Colors.RESET}")


def load_test_module(test_file: Path) -> List[Dict[str, Any]]:
    """Load tests from a Python module."""
    try:
        spec = importlib.util.spec_from_file_location(test_file.stem, test_file)
        if spec and spec.loader:
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            if hasattr(module, "get_tests"):
                return module.get_tests()
    except Exception as e:
        print(f"{Colors.RED}Error loading {test_file}: {e}{Colors.RESET}")
    return []


def main():
    """Main entry point for test runner."""
    parser = argparse.ArgumentParser(
        description="Run integration tests for Qapla Engine Tester"
    )
    parser.add_argument(
        "--filter",
        type=str,
        default="*",
        help="Filter tests by name pattern (supports wildcards)",
    )
    parser.add_argument(
        "--config",
        type=str,
        default="default",
        help="Build configuration to use (default or release)",
    )
    parser.add_argument("--test", type=str, help="Run specific test by name")
    parser.add_argument(
        "--list", action="store_true", help="List available tests without running"
    )
    parser.add_argument(
        "--skip-passed",
        action="store_true",
        help="Skip tests that passed in the last run",
    )

    args = parser.parse_args()

    # Load all tests from test directories
    all_tests: List[Dict[str, Any]] = []
    test_integration_dir = Path("test/integration")

    for test_dir in test_integration_dir.iterdir():
        if test_dir.is_dir():
            test_file = test_dir / f"{test_dir.name}_tests.py"
            if test_file.exists():
                tests = load_test_module(test_file)
                if tests:
                    all_tests.extend(tests)

    # Filter tests
    tests_to_run: List[Dict[str, Any]] = []

    if args.test:
        tests_to_run = [t for t in all_tests if t["name"] == args.test]
        if not tests_to_run:
            print(f"{Colors.RED}Test not found: {args.test}{Colors.RESET}")
            return 1
    else:
        # Convert wildcard pattern to regex
        import re

        pattern = args.filter.replace("*", ".*").replace("?", ".")
        pattern_re = re.compile(pattern)
        tests_to_run = [t for t in all_tests if pattern_re.match(t["name"])]

    # List tests if requested
    if args.list:
        print(f"{Colors.CYAN}Available tests:{Colors.RESET}")
        print()
        for test in all_tests:
            print(f"  {Colors.GREEN}+ {test['name']}{Colors.RESET}")
            print(f"    {Colors.GRAY}{test['description']}{Colors.RESET}")
        return 0

    if not tests_to_run:
        print(f"{Colors.RED}No tests found for filter: {args.filter}{Colors.RESET}")
        return 1

    # Load cumulative results
    cumulative_results = load_previous_results()
    
    # Filter out passed tests if requested
    original_count = len(tests_to_run)
    if args.skip_passed:
        tests_to_run = [
            t for t in tests_to_run 
            if cumulative_results.get(t["name"]) != "PASSED"
        ]
        skipped_count = original_count - len(tests_to_run)
        if skipped_count > 0:
            print(f"{Colors.YELLOW}Skipping {skipped_count} tests that already passed.{Colors.RESET}")

    if not tests_to_run and original_count > 0:
        print(f"{Colors.GREEN}All selected tests have already passed. Nothing to do.{Colors.RESET}")
        return 0

    # Run tests
    print()
    print(f"{Colors.CYAN}{'=' * 40}{Colors.RESET}")
    print(f"{Colors.CYAN}  Qapla Engine Tester - Test Runner{Colors.RESET}")
    print(f"{Colors.CYAN}{'=' * 40}{Colors.RESET}")
    print()
    print(f"{Colors.YELLOW}Tests to run: {len(tests_to_run)}{Colors.RESET}")
    print()

    results = []
    passed = 0
    failed = 0

    for test in tests_to_run:
        result = invoke_test(test, args.config)

        if result:
            passed += 1
            result_text = f"✔ {test['name']} - PASSED"
            results.append({"name": test["name"], "passed": True, "text": result_text})
            cumulative_results[test["name"]] = "PASSED"
        else:
            failed += 1
            result_text = f"✘ {test['name']} - FAILED"
            results.append({"name": test["name"], "passed": False, "text": result_text})
            cumulative_results[test["name"]] = "FAILED"

        # Save updated cumulative results immediately
        save_results(cumulative_results)

    # Print summary
    print()
    print(f"{Colors.CYAN}{'=' * 40}{Colors.RESET}")
    print(f"{Colors.CYAN}  Test Results Summary{Colors.RESET}")
    print(f"{Colors.CYAN}{'=' * 40}{Colors.RESET}")
    print()

    for result in results:
        if result["passed"]:
            print(f"{Colors.GREEN}{result['text']}{Colors.RESET}")
        else:
            print(f"{Colors.RED}{result['text']}{Colors.RESET}")

    print()
    print(f"{Colors.CYAN}{'-' * 40}{Colors.RESET}")
    print(
        f"{Colors.YELLOW}Total: {len(results)} | Passed: {passed} | "
        f"Failed: {failed}{Colors.RESET}"
    )
    print()

    if failed == 0:
        print(f"{Colors.GREEN}All tests passed!{Colors.RESET}")
        return 0
    else:
        print(f"{Colors.RED}{failed} test(s) failed{Colors.RESET}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
