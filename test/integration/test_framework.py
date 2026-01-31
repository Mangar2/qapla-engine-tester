#!/usr/bin/env python3
"""Integration Test Framework for Qapla Engine Tester

Provides validation functions and test execution infrastructure.
"""

import difflib
import os
import re
import shlex
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


def validate_file_content(path: str, content_pattern: str, test_name: str, error_msg: Optional[str] = None, is_regex: bool = False) -> bool:
    """Validate that a file contains the expected content pattern."""
    if not os.path.exists(path):
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} File not found for content check: {path}")
        return False

    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()

        found = False
        if is_regex:
            found = re.compile(content_pattern).search(content) is not None
        else:
            found = content_pattern in content

        if found:
            print(f"  {Colors.GREEN}[OK]{Colors.RESET} File '{path}' has expected content")
            return True
        else:
            msg = error_msg if error_msg else f"missing content: '{content_pattern}'"
            print(
                f"  {Colors.RED}[FAIL]{Colors.RESET} File '{path}': {msg}"
            )
            return False
    except Exception as e:
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} Error reading file '{path}': {e}")
        return False


def validate_file_append_only(
    path: str, original_content: str, test_name: str
) -> bool:
    """Validate that file was only appended to, original content preserved."""
    if not os.path.exists(path):
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} File not found: {path}")
        return False

    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            current_content = f.read()

        if not current_content.startswith(original_content):
            print(
                f"  {Colors.RED}[FAIL]{Colors.RESET} File content was modified "
                f"(original content not preserved): {path}"
            )
            
            # Show unified diff (limited to first 20 lines)
            original_lines = original_content.splitlines(keepends=True)
            current_lines = current_content.splitlines(keepends=True)
            
            diff = difflib.unified_diff(
                original_lines,
                current_lines,
                fromfile="original",
                tofile="current",
                lineterm="",
            )
            
            print(f"  {Colors.YELLOW}Diff (first 20 lines):{Colors.RESET}")
            diff_lines = list(diff)
            for idx, line in enumerate(diff_lines[:20]):
                line = line.rstrip()
                if line.startswith("+++") or line.startswith("---"):
                    print(f"    {Colors.GRAY}{line}{Colors.RESET}")
                elif line.startswith("+"):
                    print(f"    {Colors.GREEN}{line}{Colors.RESET}")
                elif line.startswith("-"):
                    print(f"    {Colors.RED}{line}{Colors.RESET}")
                elif line.startswith("@@"):
                    print(f"    {Colors.CYAN}{line}{Colors.RESET}")
                else:
                    print(f"    {line}")
            
            if len(diff_lines) > 20:
                print(f"    {Colors.GRAY}... ({len(diff_lines) - 20} more lines){Colors.RESET}")
            
            return False

        if len(current_content) > len(original_content):
            added_lines = len(current_content.splitlines()) - len(
                original_content.splitlines()
            )
            print(
                f"  {Colors.GREEN}[OK]{Colors.RESET} File only appended "
                f"({added_lines} lines added): {path}"
            )
        else:
            print(
                f"  {Colors.GREEN}[OK]{Colors.RESET} File content preserved: {path}"
            )

        return True
    except Exception as e:
        print(
            f"  {Colors.RED}[FAIL]{Colors.RESET} Error reading file {path}: {e}"
        )
        return False


def invoke_test(test: Dict[str, Any], config_name: str = "default") -> bool:
    """Execute a single test and validate results."""
    print()
    print(f"  {Colors.CYAN}Test: {test['name']}{Colors.RESET}")

    # Copy source files to target location if specified
    source_file_configs = []
    if "source_files" in test and test["source_files"]:
        import shutil
        for config in test["source_files"]:
            source = config["source"]
            target = config["target"]
            if os.path.exists(source):
                try:
                    shutil.copy2(source, target)
                    source_file_configs.append(config)
                except Exception as e:
                    print(
                        f"  {Colors.YELLOW}[WARN]{Colors.RESET} "
                        f"Failed to copy {source} to {target}: {e}"
                    )

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
    args = shlex.split(test["args"], posix=True)
    cmd = [f"build/{config_name}/qapla-engine-tester.exe"] + args

    print(f"  {Colors.GRAY}Running: {' '.join(cmd)}{Colors.RESET}")
    print()

    # Run command
    exit_code = -1
    try:
        process = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="ignore", bufsize=1
        )
        
        if process.stdout:
            for line in iter(process.stdout.readline, ""):
                print(f"    {line.rstrip()}")
        
        process.wait()
        exit_code = process.returncode
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
        elif validator_type == "fileContent":
            result = validate_file_content(
                path=validator["path"],
                content_pattern=validator["content"],
                test_name=test["name"],
                error_msg=validator.get("message"),
                is_regex=validator.get("isRegex", False)
            )
        elif validator_type == "fileExists":
            result = validate_file_exists(validator["path"], test["name"])
        elif validator_type == "fileAppendOnly":
            file_path = validator["path"]
            # Find source file for this target
            source_path = None
            for config in source_file_configs:
                if config["target"] == file_path:
                    source_path = config["source"]
                    break
            
            if source_path and os.path.exists(source_path):
                try:
                    with open(source_path, "r", encoding="utf-8", errors="ignore") as f:
                        original_content = f.read()
                    result = validate_file_append_only(
                        file_path, original_content, test["name"]
                    )
                except Exception as e:
                    print(
                        f"  {Colors.YELLOW}[SKIP]{Colors.RESET} "
                        f"Failed to read source file {source_path}: {e}"
                    )
                    result = True
            else:
                print(
                    f"  {Colors.YELLOW}[SKIP]{Colors.RESET} "
                    f"No source file configured for: {file_path}"
                )
                result = True
        else:
            print(
                f"  {Colors.YELLOW}[SKIP]{Colors.RESET} Unknown validator: {validator_type}"
            )

        if not result:
            all_passed = False

    # Keep modified files as test results if specified
    if source_file_configs:
        import shutil
        for config in source_file_configs:
            target = config["target"]
            keep_modified = config.get("keep_modified")
            if keep_modified and os.path.exists(target):
                try:
                    shutil.copy2(target, keep_modified)
                    print(
                        f"  {Colors.CYAN}[INFO]{Colors.RESET} "
                        f"Modified file saved to: {keep_modified}"
                    )
                except Exception as e:
                    print(
                        f"  {Colors.YELLOW}[WARN]{Colors.RESET} "
                        f"Failed to save modified file: {e}"
                    )

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
