#!/usr/bin/env python3
"""Integration Test Framework for Qapla Engine Tester

Provides validation functions and test execution infrastructure.
"""

import os
import re
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional


class Colors:
    """ANSI color codes for terminal output."""

    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    CYAN = "\033[96m"
    GRAY = "\033[90m"
    RESET = "\033[0m"


def validate_exit_code(actual_code: int, expected_code: int, test_name: str) -> bool:
    """Validate that exit code matches expected value."""
    if actual_code == expected_code:
        print(f"  {Colors.GREEN}[OK]{Colors.RESET} Exit Code: {actual_code}")
        return True
    else:
        print(
            f"  {Colors.RED}[FAIL]{Colors.RESET} Exit Code: {actual_code} "
            f"(expected: {expected_code})"
        )
        return False


def validate_log_files(
    path: str,
    pattern: str,
    expected_count: int,
    content_pattern: Optional[str] = None,
    test_name: str = "",
) -> bool:
    """Validate log file count and optionally their content."""
    if not os.path.exists(path):
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} Log directory not found: {path}")
        return False

    # Convert glob pattern to regex for matching
    regex_pattern = pattern.replace("*", ".*").replace("?", ".")
    log_files = [
        f
        for f in os.listdir(path)
        if os.path.isfile(os.path.join(path, f)) and re.match(regex_pattern, f)
    ]
    actual_count = len(log_files)

    if actual_count != expected_count:
        print(
            f"  {Colors.RED}[FAIL]{Colors.RESET} Log file count: {actual_count} "
            f"(expected: {expected_count})"
        )
        return False

    print(
        f"  {Colors.GREEN}[OK]{Colors.RESET} Log files: {actual_count} found "
        f"(expected: {expected_count})"
    )

    if content_pattern:
        all_have_content = True
        content_regex = re.compile(content_pattern)
        for filename in log_files:
            filepath = os.path.join(path, filename)
            try:
                with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                    content = f.read()
                if not content_regex.search(content):
                    print(
                        f"  {Colors.RED}[FAIL]{Colors.RESET} Log file '{filename}' "
                        f"missing content: '{content_pattern}'"
                    )
                    all_have_content = False
                else:
                    print(
                        f"  {Colors.GREEN}[OK]{Colors.RESET} Log file '{filename}' "
                        f"has expected content"
                    )
            except Exception as e:
                print(
                    f"  {Colors.RED}[FAIL]{Colors.RESET} Error reading '{filename}': {e}"
                )
                all_have_content = False
        return all_have_content

    return True


def validate_file_exists(path: str, test_name: str) -> bool:
    """Validate that a file exists."""
    if os.path.exists(path):
        print(f"  {Colors.GREEN}[OK]{Colors.RESET} File exists: {path}")
        return True
    else:
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} File not found: {path}")
        return False


def invoke_test(test: Dict[str, Any]) -> bool:
    """Execute a single test and validate results."""
    print()
    print(f"  {Colors.CYAN}Test: {test['name']}{Colors.RESET}")

    # Run cleanup if specified
    if "cleanup" in test and test["cleanup"]:
        cleanup_path = test["cleanup"]
        if os.path.exists(cleanup_path):
            import shutil

            shutil.rmtree(cleanup_path, ignore_errors=True)

    # Ensure log directory exists
    log_path = test.get("log_path", "log")
    os.makedirs(log_path, exist_ok=True)

    # Build command
    args = test["args"].split()
    cmd = ["build/default/qapla-engine-tester.exe"] + args

    print(f"  {Colors.GRAY}Running: {' '.join(cmd)}{Colors.RESET}")
    print()

    # Run command
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, encoding="utf-8", errors="ignore"
        )
        exit_code = result.returncode

        # Print output with indentation
        for line in result.stdout.splitlines():
            print(f"    {line}")
        if result.stderr:
            for line in result.stderr.splitlines():
                print(f"    {line}")
    except Exception as e:
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} Failed to run command: {e}")
        return False

    print()

    # Run validators
    all_passed = True
    for validator in test.get("validators", []):
        result = False
        validator_type = validator["type"]

        if validator_type == "exitCode":
            result = validate_exit_code(
                exit_code, validator["expected"], test["name"]
            )
        elif validator_type == "logFiles":
            full_path = os.path.join(log_path, validator.get("path", ""))
            result = validate_log_files(
                path=full_path,
                pattern=validator["pattern"],
                expected_count=validator["count"],
                content_pattern=validator.get("content"),
                test_name=test["name"],
            )
        elif validator_type == "fileExists":
            result = validate_file_exists(validator["path"], test["name"])
        else:
            print(
                f"  {Colors.YELLOW}[SKIP]{Colors.RESET} Unknown validator: {validator_type}"
            )

        if not result:
            all_passed = False

    print()
    if all_passed:
        print(f"  {Colors.GREEN}[PASS]{Colors.RESET} {test['name']}")
        return True
    else:
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} {test['name']}")
        return False


def remove_test_directory(path: str, force: bool = False) -> None:
    """Remove test directory if it exists."""
    if os.path.exists(path):
        import shutil

        shutil.rmtree(path, ignore_errors=force)


def clear_log_directory(path: str = "log", pattern: str = "*.log") -> None:
    """Clear log files matching pattern in directory."""
    if not os.path.exists(path):
        return

    regex_pattern = pattern.replace("*", ".*").replace("?", ".")
    for filename in os.listdir(path):
        if os.path.isfile(os.path.join(path, filename)) and re.match(
            regex_pattern, filename
        ):
            try:
                os.remove(os.path.join(path, filename))
            except:
                pass
