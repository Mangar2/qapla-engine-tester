---
name: integration-test-framework
description: "Conventions and API of the Qapla integration test framework (test_runner.py / test_framework.py). USE when: writing, reviewing, or planning integration tests in test/integration/. Covers test discovery, test dictionary format, validator types, and runner CLI."
---

# Integration Test Framework

## Test Discovery

- Runner scans `test/integration/` for subdirectories.
- Each subdirectory must contain `<dirname>_tests.py` exporting `get_tests() -> List[Dict[str, Any]]`.
- Example: `test/integration/epd/epd_tests.py`

## Runner CLI

```
python test/integration/test_runner.py [options]
```

| Flag | Description |
|---|---|
| `--config <name>` | Build config: `default`, `release`, `unit` (selects `build/<config>/qapla-engine-tester.exe`) |
| `--filter <pattern>` | Wildcard pattern (`*`/`?`) matched against test names |
| `--test <name>` | Run single test by exact name |
| `--list` | List available tests without running |
| `--skip-passed` | Skip tests with PASSED in `test/integration/test_results.log` |

## Test Dictionary

Each test returned by `get_tests()` is a dict with:

### Required Keys

| Key | Type | Description |
|---|---|---|
| `name` | `str` | Unique test identifier |
| `description` | `str` | Human-readable description |
| `args` | `str` | CLI arguments passed to binary (shell-quoted) |

### Optional Keys

| Key | Type | Default | Description |
|---|---|---|---|
| `validators` | `list` | `[]` | List of validator dicts (see below) |
| `cleanup` | `str` | — | Directory removed before test runs |
| `log_path` | `str` | `"log"` | Base directory for log validators |
| `input` | `str` or `list` | — | Stdin data sent to binary. List items are joined with `\n`. |
| `source_files` | `list` | — | Files copied before test (see Source Files) |

### Source Files

Each entry: `{"source": path, "target": path, "keep_modified": optional_path}`

- `source` is copied to `target` before execution.
- `keep_modified`: if set, the modified `target` is saved here after test.
- Original content of `source` is used by `fileAppendOnly` validator.

## Validator Types

ALL validators must pass for a test to pass. They run sequentially.

### exitCode

```python
{"type": "exitCode", "expected": <int>}
```

Checks process exit code. See CLI return codes in copilot-instructions.md.

### stdout

```python
{"type": "stdout", "content": "<pattern>", "isRegex": False}
```

Checks stdout contains literal substring (default) or regex match.

### logFiles

```python
{"type": "logFiles", "path": "<subdir>", "pattern": "*.log", "count": <int>, "content": "<regex>"}
```

- `path`: subdirectory relative to `log_path` (empty string = log_path root)
- `pattern`: glob pattern (`*` → `.*`, `?` → `.`)
- `count`: exact number of matching files required
- `content` (optional): regex that every matching file must contain

### fileContent

```python
{"type": "fileContent", "path": "<file>", "content": "<pattern>", "isRegex": False, "message": "<custom error>"}
```

Checks file contains literal substring or regex. Optional custom error message.

### fileExists

```python
{"type": "fileExists", "path": "<file>"}
```

Checks file exists at path.

### fileAppendOnly

```python
{"type": "fileAppendOnly", "path": "<file>"}
```

Validates file was only appended to — original content from `source_files` must be preserved at start. Skipped if no `source_files` entry matches.

## Execution Lifecycle

1. **Setup**: Copy `source_files`, remove `cleanup` dir, create `log_path`
2. **Run**: Execute `build/<config>/qapla-engine-tester.exe <args>`, send `input` if set
3. **Validate**: Run all validators sequentially
4. **Persist**: Save result to `test_results.log` immediately
5. **Post**: Copy modified files to `keep_modified` paths

## Minimal Example

```python
def get_tests() -> List[Dict[str, Any]]:
    return [
        {
            "name": "epd-basic-run",
            "description": "EPD functional test - expects basic success",
            "args": "--settingsfile=test/integration/epd/test-epd.ini --epd minsuccess=100",
            "log_path": "test/integration/log/epd",
            "validators": [
                {"type": "exitCode", "expected": 13},
                {"type": "logFiles", "path": "", "pattern": "epd-report*.log", "count": 1,
                 "content": "Finished EPD test for engine: qapla-3.2"},
            ],
            "cleanup": "test/integration/log/epd",
        },
    ]
```
