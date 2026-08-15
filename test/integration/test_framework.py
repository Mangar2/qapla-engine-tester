#!/usr/bin/env python3
"""Integration Test Framework for Qapla Engine Tester

Provides validation functions and test execution infrastructure.
"""

import difflib
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


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


def validate_stdout(actual_stdout: str, pattern: str, is_regex: bool = False) -> bool:
    """Validate that stdout contains the expected pattern."""
    found = False
    if is_regex:
        found = re.compile(pattern).search(actual_stdout) is not None
    else:
        found = pattern in actual_stdout

    if found:
        print(f"  {Colors.GREEN}[OK]{Colors.RESET} stdout has expected content")
        return True
    else:
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} stdout missing pattern: '{pattern}'")
        return False


def resolve_tester_binary(config_name: str) -> str:
    """Resolve tester binary path for current platform or explicit override.

    The path is made absolute: Windows excludes the current directory from the
    executable search when NoDefaultCurrentDirectoryInExePath is set, so a
    relative binary path would not be found there.
    """
    override_binary = os.getenv("QAPLA_IT_BINARY", "").strip()
    if override_binary:
        return str(Path(override_binary).resolve())

    binary_suffix = ".exe" if os.name == "nt" else ""
    return str(Path(f"build/{config_name}/qapla-engine-tester{binary_suffix}").resolve())


def platform_suffix() -> str:
    """Returns the platform tag used to pick engine binaries for this OS."""
    if os.name == "nt":
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    return "linux"


def materialize_platform_engines_files() -> None:
    """Selects the OS-appropriate engine binaries for every fixture, in one place.

    Engine binaries differ per OS (a Windows .exe, a Linux ELF, a macOS Mach-O),
    but the fixtures all reference the same stable filenames, engines.ini and
    engines-short.ini, via --enginesfile= or an embedded enginesfile= entry. This
    copies the current platform's template (engines.<os>.ini / engines-short.<os>.ini)
    onto those stable filenames before any test runs, so the platform difference
    lives only in the template files, not in every individual fixture.
    """
    suffix = platform_suffix()
    engines_dir = Path("test/integration/engines")
    for base_name in ("engines", "engines-short"):
        source = engines_dir / f"{base_name}.{suffix}.ini"
        target = engines_dir / f"{base_name}.ini"
        if not source.exists():
            print(
                f"{Colors.YELLOW}Warning: no {source} template for platform "
                f"'{suffix}'; leaving {target} untouched.{Colors.RESET}"
            )
            continue
        shutil.copyfile(source, target)


def apply_engines_override(args: List[str]) -> List[str]:
    """Apply optional engines file override from environment."""
    override_engines_file = os.getenv("QAPLA_IT_ENGINESFILE", "").strip()
    if not override_engines_file:
        return args

    updated_args: List[str] = []
    for arg_value in args:
        if arg_value.startswith("--enginesfile="):
            updated_args.append(f"--enginesfile={override_engines_file}")
        else:
            updated_args.append(arg_value)
    return updated_args


def _parse_ini_engine_sections(path: Path) -> Dict[str, Dict[str, str]]:
    """Parses every [engine] section of an ini file into {name: {key: value}}."""
    sections: Dict[str, Dict[str, str]] = {}
    current: Optional[Dict[str, str]] = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if line == "[engine]":
            current = {}
            continue
        if line.startswith("[") and line.endswith("]"):
            current = None
            continue
        if current is not None and "=" in line:
            key, _, value = line.partition("=")
            current[key.strip()] = value.strip()
            if key.strip().lower() == "name":
                sections[value.strip()] = current
    return sections


_engine_config_cache: Dict[str, Dict[str, Dict[str, str]]] = {}


def get_engine_config(name: str) -> Dict[str, str]:
    """Returns the OS-resolved configuration (cmd=, proto=, ...) for a named engine.

    This is the single place that knows engine configuration - it reads
    test/integration/engines/engines.<os>.ini, the tracked, per-platform source of
    truth for what a named engine looks like on this OS. Callers ask for an engine
    by its logical name and get back its resolved fields; they never hardcode a
    cmd= path themselves, so there is nothing to keep in sync across platforms.
    """
    suffix = platform_suffix()
    if suffix not in _engine_config_cache:
        path = Path("test/integration/engines") / f"engines.{suffix}.ini"
        _engine_config_cache[suffix] = _parse_ini_engine_sections(path)
    engines = _engine_config_cache[suffix]
    if name not in engines:
        raise KeyError(
            f"Engine '{name}' not found in engines.{suffix}.ini "
            f"(known engines: {sorted(engines.keys())})"
        )
    return dict(engines[name])


def engine_ini_block(name: str, as_: Optional[str] = None, **overrides: str) -> str:
    """Renders a single [engine] ini block for `name`, resolved for the current OS.

    `as_` overrides the name= shown to the tool (e.g. so one binary can play under
    two identities in a self-play fixture). Extra keyword args (e.g. tc="inf")
    are applied on top of the resolved configuration.
    """
    config = get_engine_config(name)
    if as_:
        config["name"] = as_
    config.update(overrides)
    lines = ["[engine]"] + [f"{key}={value}" for key, value in config.items()]
    return "\n".join(lines)


def apply_engine_refs(args: List[str], engine_refs: Optional[List[Dict[str, str]]], log_path: str) -> List[str]:
    """Appends OS-resolved [engine] blocks to a fixture's --settingsfile=, in place.

    Fixtures that need one or more engines keep a single, platform-independent ini
    file with no [engine] section at all, and declare their engines via the test's
    "engine_refs" list instead (see engine_ini_block for the field meaning). This
    reads that ini, appends the resolved blocks, writes the result next to the
    test's own log output, and points --settingsfile= at the resolved copy - the
    settings-file-defines-the-engine mechanism under test is unchanged, only the
    engine data's source is centralized instead of duplicated per platform.
    """
    if not engine_refs:
        return args

    settingsfile_index = next(
        (i for i, a in enumerate(args) if a.startswith("--settingsfile=")), None
    )
    if settingsfile_index is None:
        raise ValueError("engine_refs given but args has no --settingsfile= to extend")

    original_path = Path(args[settingsfile_index][len("--settingsfile="):])
    blocks = "\n\n".join(engine_ini_block(**ref) for ref in engine_refs)
    resolved_content = original_path.read_text(encoding="utf-8").rstrip() + "\n\n" + blocks + "\n"

    os.makedirs(log_path, exist_ok=True)
    resolved_path = Path(log_path) / f"_resolved-{original_path.name}"
    resolved_path.write_text(resolved_content, encoding="utf-8")

    updated_args = list(args)
    updated_args[settingsfile_index] = f"--settingsfile={resolved_path}"
    return updated_args


def format_duration(seconds: float) -> str:
    """Formats a runtime for humans: seconds below a minute, m:ss above."""
    if seconds < 60.0:
        return f"{seconds:.2f}s"
    minutes = int(seconds // 60)
    return f"{minutes}m {seconds - minutes * 60:04.1f}s"


def invoke_test(test: Dict[str, Any], config_name: str = "default") -> Tuple[bool, float]:
    """Execute a single test and validate results.

    Returns the pass/fail verdict together with the runtime of the tested
    process in seconds. Runtime is a test property worth tracking: the suite is
    built around short, parametrized runs, so a test that suddenly takes much
    longer is a finding even while it still passes.
    """
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

    # Copy source files to their working location. This runs after the cleanup so
    # that working copies inside the log directory survive it.
    source_file_configs = []
    if "source_files" in test and test["source_files"]:
        import shutil
        for config in test["source_files"]:
            source = config["source"]
            target = config["target"]
            if os.path.exists(source):
                try:
                    target_directory = os.path.dirname(target)
                    if target_directory:
                        os.makedirs(target_directory, exist_ok=True)
                    shutil.copy2(source, target)
                    source_file_configs.append(config)
                except Exception as e:
                    print(
                        f"  {Colors.YELLOW}[WARN]{Colors.RESET} "
                        f"Failed to copy {source} to {target}: {e}"
                    )
            else:
                print(
                    f"  {Colors.YELLOW}[WARN]{Colors.RESET} "
                    f"Source file not found: {source}"
                )

    # Build command
    args = shlex.split(test["args"], posix=True)
    args = apply_engines_override(args)
    args = apply_engine_refs(args, test.get("engine_refs"), log_path)
    tester_binary = resolve_tester_binary(config_name)
    cmd = [tester_binary] + args

    exit_code = -1
    stdout_output = ""
    start_time = time.monotonic()
    duration_seconds = 0.0

    # Preparation runs, for tests whose subject needs state that a previous run
    # produced - writing a state file and reading it back, for example. Their
    # output is not validated: only the run under test is. Their exit code is
    # printed because a preparation that did not do its job would otherwise show
    # up as a confusing failure of the real command.
    setup_args = test.get("setup_args")
    if setup_args:
        for setup_arg_string in ([setup_args] if isinstance(setup_args, str) else setup_args):
            setup_cmd = [tester_binary] + apply_engines_override(
                shlex.split(setup_arg_string, posix=True)
            )
            print(f"  {Colors.GRAY}Preparing: {' '.join(setup_cmd)}{Colors.RESET}")
            setup_result = subprocess.run(
                setup_cmd, capture_output=True, text=True, encoding="utf-8", errors="ignore"
            )
            print(
                f"  {Colors.GRAY}Preparation finished with exit code "
                f"{setup_result.returncode}{Colors.RESET}"
            )

    print(f"  {Colors.GRAY}Running: {' '.join(cmd)}{Colors.RESET}")
    print()

    try:
        process = subprocess.Popen(
            cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="ignore", bufsize=1
        )
        
        input_data = test.get("input")
        if input_data:
            if isinstance(input_data, list):
                input_str = "\n".join(input_data) + "\n"
            else:
                input_str = input_data
            stdout_output, _ = process.communicate(input=input_str)
            for line in stdout_output.splitlines():
                print(f"    {line}")
        else:
            if process.stdout:
                for line in iter(process.stdout.readline, ""):
                    print(f"    {line.rstrip()}")
                    stdout_output += line
        
        process.wait()
        exit_code = process.returncode
        duration_seconds = time.monotonic() - start_time
    except Exception as e:
        duration_seconds = time.monotonic() - start_time
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} Failed to run command: {e}")
        return False, duration_seconds

    print()
    print(f"  {Colors.GRAY}Runtime: {format_duration(duration_seconds)}{Colors.RESET}")

    # Run validators
    all_passed = True
    for validator in test.get("validators", []):
        result = False
        validator_type = validator["type"]

        if validator_type == "exitCode":
            result = validate_exit_code(
                exit_code, validator["expected"], test["name"]
            )
        elif validator_type == "stdout":
            result = validate_stdout(
                stdout_output,
                validator["content"],
                validator.get("isRegex", False)
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
    runtime_text = f" ({format_duration(duration_seconds)})"
    if all_passed:
        print(f"  {Colors.GREEN}[PASS]{Colors.RESET} {test['name']}{runtime_text}")
        return True, duration_seconds
    else:
        print(f"  {Colors.RED}[FAIL]{Colors.RESET} {test['name']}{runtime_text}")
        return False, duration_seconds


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
