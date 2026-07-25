# Plan: Simplify the engine-tester's JSON handling using `src/json`

## Motivation

The engine-tester is the base of the qapla-engine-gui, which will add an LLM
interface on top of the MCP layer. That means: arbitrary LLM-generated text
flowing through JSON payloads (worst case for the current incomplete string
escaping), parsing of possibly malformed LLM output (needs the strict parser
with `try_parse` and error offsets), and a growing number of MCP
tools/payloads in two projects (each currently paying the `make*` boilerplate
and string-round-trip tax). This migration is the prerequisite for that work,
not a cosmetic refactor.

## Guiding principle — read this first

**The goal is never "use the new JSON library". The goal is simpler, smaller,
clearer code. The library (`src/json`, `mqtt::json::JsonValue`, see
`src/json/SPEC.md`) is only the means.**

Consequences, binding for every step of this plan:

1. **No 1:1 translation.** A migration step that merely swaps
   `JsonHelper::makeString(x)` for `JsonValue{std::string(x)}` is a failed
   step. Every migrated function must come out shorter and more readable than
   before, using the library's JavaScript-like semantics:
   - implicit constructors from `bool`, `double`, `std::string`, `const char*`
     — the whole `make*` layer disappears,
   - `operator[]` auto-creates objects/keys — the
     "create `Object`, fill via `make*`, move-insert" three-step becomes one
     assignment per field: `root["active_job"]["job_id"] = job.jobId;`,
   - `at(...)`/`contains` replace iterator `find` dances.
   Only ints need `static_cast<double>`; accept that, do not build a helper
   zoo around it.
2. **Rebuild the structures that were written around the old helper.** Because
   the old `JsonHelper` made passing trees awkward, several interfaces exchange
   *serialized JSON strings internally* and re-parse them. These string-shaped
   interfaces are part of the old JSON code and must be rebuilt, not wrapped
   (inventory in 1.4).
3. **Serialize once, parse once.** `stringify()` happens exactly once per
   output, at the real boundary (MCP stdout channel, log file, persisted INI
   line). `parse` happens exactly once per input, at the real input boundary
   (MCP stdin, persisted capability line). Internal interfaces pass
   `JsonValue` trees. After the migration, an internal serialize→parse round
   trip is a defect.
4. When old behavior and simpler code conflict, prefer simpler code and adapt
   the affected test (the known cases are listed in section 8 — anything else
   must be surfaced, not silently decided).

Replacing 100% of the engine-tester's own JSON code (explicit helper and
implicit hand-written JSON) is the *consequence* of the above, and is verified
by the sweeps in 1.5.

## 1. Inventory — what gets removed or rebuilt

### 1.1 Explicit JSON library (deleted at the end)

| File | Content |
|---|---|
| `src/base-elements/json-helper.h` | `Mcp::JsonValue` (variant-based), `Mcp::JsonHelper` (make*, parse, serialize) |
| `src/base-elements/json-helper.cpp` | Hand-written recursive parser + serializer (incomplete string escaping: only `\"`, `\\`, `\n`) |

### 1.2 Consumers of `JsonHelper` / `Mcp::JsonValue` (rewritten idiomatically)

- `src/mcp/mcp-server.cpp` — JSON-RPC request/response handling
- `src/mcp/mcp-message-channel.{h,cpp}` — stdio framing + parse/serialize
- `src/mcp/mcp-background-tools.{h,cpp}` — runner status JSON
- `src/mcp/job-scheduler.{h,cpp}` — `queueStatusJson()`, `finishedResultsJson()`
- `src/mcp/mcp-converter.{h,cpp}` — `JsonValue` → `Settings::Value`
- `src/mcp/mcp-engine-tool.{h,cpp}` — tool argument access
- `src/mcp/mcp-schema-builder.{h,cpp}` — input schemas (`JsonValue{ .data = ... }` aggregate init disappears)
- `src/mcp/settings-reporter.{h,cpp}`
- `src/cli/app-runner.cpp` — status objects for all task types
- `src/base-elements/base-logger.cpp` — text/status payloads for MCP logging
- `src/base-elements/table-format.{h,cpp}` — `TableFormat::toJson`

### 1.3 Implicit, hand-written JSON (rewritten)

- `src/game-manager/tournament-result.cpp` — `getRatingTableJson()` (~line
  563), `getOutcomeJson()` (~line 588): JSON via `std::format` concatenation,
  **no string escaping** (engine names with `"` or `\` produce invalid JSON).
- `src/engine-handling/engine-capability.cpp` — hand-written serializer
  (~line 41) and `parseEngineOption` tokenizer (~line 100–200) for
  `EngineOption`. Serialized lines are **persisted** in the capability INI
  cache → read compatibility required (5.3).
- `src/mcp/mcp-message-channel.cpp` (~line 140) — quote/escape-aware scanning
  to extract a complete JSON document from the stream. This is message
  *framing*, not value parsing: keep the framing, move the parse to
  `JsonValue::parse`.

### 1.4 Structures shaped by the old helper (interfaces to rebuild)

These exist only because the old helper made tree-passing awkward; each is an
internal serialize→parse round trip or a string-typed interface that must
become `JsonValue`-typed:

| Site | Today | Rebuild |
|---|---|---|
| `TournamentResult::getRatingTableJson/getOutcomeJson` → `app-runner.cpp` `parseJsonText(...)` (lines ~95, ~108) | returns serialized string; app-runner re-parses it into the status tree | `TournamentResult` returns `JsonValue`; string variants disappear |
| `AppRunner::getTaskStatusJson`/`getStatus` → `mcp-background-tools.cpp` `createCombinedControlStatus` (~line 47) | returns serialized string; background-tools re-parses to add `job_queue`, re-serializes | `AppRunner` exposes `JsonValue getTaskStatus(...)`; callers add fields directly; serialization only at the MCP boundary |
| `QueueJob::taskStatusJson` (`job-scheduler.h` ~line 81) re-parsed in `finishedResultsJson` (~line 215) | stores serialized string, parses on read | store `JsonValue taskStatus` |
| `TableFormat::toJson` → `base-logger` → `mcpCallback_(string_view, string_view)` | payload travels as pre-serialized string through the logger into the MCP layer | trace the payload path; pass `JsonValue` (or keep `std::string` only for the final channel write). Decide the callback signature here — serialized exactly once, at the channel |
| `mcp-server.cpp` ~line 797/962 | `serialize(finishedResultsJson())` then embeds the string in the response | embed the `JsonValue` tree in the response object; one `stringify()` when the message is written |

If further round trips surface during implementation, rebuild them under the
same rule (3. serialize once, parse once).

### 1.5 Verification sweeps (must be clean at the end)

```
grep -rn "json-helper\|JsonHelper\|Mcp::JsonValue" src --exclude-dir=json
grep -rnE '\\"[a-zA-Z_]+\\":' src --exclude-dir=json     # hand-built JSON keys
grep -rn "parse" src/mcp src/cli --include=*.cpp | grep -i json   # manual audit: parses only at input boundaries
```

## 2. Design decisions

### 2.1 Namespace

`src/json` lives in namespace `mqtt::json` (YAHA origin). One small header
keeps call sites readable and the module drop-in updatable from upstream:

```cpp
// src/base-elements/qapla-json.h
#pragma once
#include "../json/json_value.h"
#include "../json/json_error.h"
namespace QaplaTester { namespace Json = mqtt::json; }
```

Migrated code uses `Json::JsonValue`. `mqtt::json` itself stays untouched.

### 2.2 Target idioms (patterns, not 1:1 mappings)

| Old pattern | Target idiom |
|---|---|
| `Object o; o["k"] = JsonHelper::makeString(s); ... makeObject(std::move(o))` | `auto v = JsonValue::object(); v["k"] = s;` — one line per field, nesting via `v["a"]["b"] = ...` |
| `Array a; a.push_back(makeObject(...)); makeArray(std::move(a))` | `auto v = JsonValue::array(); v.push_back(...)` or index assignment `v[i] = ...` |
| `JsonValue{ .data = std::string("object") }` (schema-builder) | `prop["type"] = "object";` |
| `it = obj.find("k"); if (it != end && it->second.isBool()) ...` | `v.is_object() && v.as_object().contains("k")` + `v.at("k").as_boolean()`, or a single local accessor if a file repeats the pattern |
| `serialize(v)` passed through internal interfaces | pass `JsonValue`; `stringify()` once at the boundary |
| `JsonHelper::parse(view)` on internally produced strings | delete the parse — the producer now hands over the tree |

Expected effect (review yardstick, not hard numbers): the status-building
functions in `job-scheduler` and `app-runner` shrink to roughly half;
`mcp-schema-builder` and `table-format` lose all construction boilerplate;
`tournament-result` and `engine-capability` lose their hand-rolled
serializer/parser entirely.

### 2.3 Error handling

- New parser throws `mqtt::json::JsonException` (old code:
  `std::runtime_error`). Audit every parse call site — after the rebuild in
  1.4 only *real input boundaries* parse at all:
  - MCP stdin (`mcp-message-channel`/`mcp-server`): malformed message →
    JSON-RPC parse error (-32700), server keeps running → `try_parse`.
  - Persisted capability lines: map failure to the current
    `std::invalid_argument` semantics or adapt callers.
- Old `parse("")` returned null; new `parse` throws → boundaries that may see
  empty input use `try_parse` or an explicit check.

## 3. Phase 0 — Build integration of `src/json`

The module is not yet buildable inside this repo:

1. `CMakeLists.txt`:
   - add `src` to the include directories of `qapla-engine-tester` and
     `qapla-lib` (module-internal includes are `"json/json_value.h"`),
   - exclude `src/json/test/.*\.cpp$` from `SOURCES` and `LIB_SOURCES`
     (Catch2 code must not link into the main executable),
   - add `src/json/test/*.cpp` to `UNIT_TEST_SOURCES`.
2. Build main target and unit test target (keep the MSVC project in sync if
   applicable).

**Exit criterion:** clean build of `qapla-engine-tester` and
`qapla-unit-tests` with `-DBUILD_UNIT_TESTS=ON`.

## 4. Phase 1 — JSON library unit tests + wire-format pin

1. Run `ctest`; all cases in `src/json/test/json_value_test.cpp` pass.
2. Add one small **wire-format pin test** (permanent, not throwaway): integral
   doubles stringify without decimal point (`42`, not `42.0`), and one case
   each for string escaping of `"` `\` `\n` `\t`. This pins the only real
   compatibility risk for MCP clients before any consumer changes. No
   old-vs-new double-serialization scaffold — structural asserts on the new
   output plus the integration suite cover the rest.

**Exit criterion:** `ctest` green including `src/json` tests and the pin test.

## 5. Phase 2 — Rebuild, module by module

Each step: rebuild → build → run unit tests → commit. Leaf utilities first,
`json-helper.{h,cpp}` deleted last. Every step follows the guiding principle:
rewrite idiomatically (2.2), rebuild string-shaped interfaces (1.4). A step
whose diff leaves the touched functions as long as before is redone.

### 5.1 Base elements

1. `table-format`: `toJson` → returns `JsonValue` (rename accordingly, e.g.
   `toJsonValue`); `formatCellForJson` collapses via implicit constructors.
2. `base-logger`: payload builders on `operator[]`; decide the
   `mcpCallback_` signature per 1.4 (serialize once at the channel).

Unit test: `table-format-json-test.cpp` — column objects, row arrays, numeric
rounding, strings containing quotes/newlines.

### 5.2 Tournament result

`getRatingTableJson`/`getOutcomeJson` → `getRatingTable`/`getOutcome`
returning `JsonValue`; app-runner's `parseJsonText` round trip is deleted in
the same step (they are one structure). Field names stay (`type`, `data`,
`rank`, `name`, `elo`, ...). `elo`/`score` currently use `{:.1f}`/`{:.2f}` —
store pre-rounded doubles and assert the exact output in the test.

Unit test: `tournament-result-json-test.cpp` — including an engine name with
`"` and `\` (behavior fix: output becomes valid JSON where it previously was
not).

### 5.3 Engine capability (persisted format!)

Replace serializer and `parseEngineOption` tokenizer with `JsonValue`.

- **Read compatibility:** lines written by the old serializer
  (`{"name": "...", ...}` with spaces, `min`/`max` numbers, `defaultValue`
  string, `vars` string array) are valid JSON and must parse — unit test with
  verbatim old-format sample lines.
- **Write format:** new `stringify()` is compact (no spaces) — acceptable;
  document in `CHANGELOG.md`.
- Map `JsonException` per 2.3.

Unit test: `engine-capability-json-test.cpp` — round-trip new format, parse
old format, optional fields present/absent, `vars`, error cases.

### 5.4 MCP layer

Order (dependency direction):

1. `mcp-converter` (+ test: JSON → `Settings::Value` for bool/int/float/
   string, type-mismatch errors)
2. `mcp-schema-builder` — aggregate-init pattern replaced by `operator[]`
   assignments (+ test: schema of one representative tool: `type`,
   `properties`, `required`)
3. `job-scheduler` — status functions rewritten idiomatically;
   `QueueJob::taskStatusJson` becomes `JsonValue taskStatus` (1.4)
   (+ test on the JSON shape)
4. `settings-reporter`, `mcp-background-tools` (`createCombinedControlStatus`
   loses its parse once `AppRunner` hands over trees — coordinate with 5.5),
   `mcp-engine-tool`
5. `mcp-message-channel` — framing kept; parse via `try_parse`, malformed
   input → JSON-RPC parse error
6. `mcp-server` — embeds result trees instead of pre-serialized strings (1.4);
   one `stringify()` where the message is written

Unit test: `mcp-json-roundtrip-test.cpp` — canonical JSON-RPC request parsed,
dispatched values accessed, response serialized and re-parsed with the library
(self-consistency, no string comparison).

### 5.5 CLI

`app-runner.cpp`: status builders (`createSprtStatus`,
`createTournamentStatus`, ...) rewritten idiomatically; `getTaskStatusJson`/
`getStatus` return `JsonValue` (1.4); `parseJsonText` helper deleted. Thin
string wrappers remain only where a caller genuinely writes to a boundary.

Unit test: status JSON contains the documented keys with correct types.

### 5.6 Deletion and final sweep

- Delete `src/base-elements/json-helper.{h,cpp}`.
- Run the sweeps from 1.5, including the round-trip audit.

**Exit criterion:** no reference to old JSON code; no internal
serialize→parse round trip; full build green.

## 6. Phase 3 — Testing

### 6.1 Unit tests

- New tests from Phase 2 live in `src/test-system/unit/` (Catch2, tag
  `[json-migration]` + module tag), running in the existing
  `qapla-unit-tests` target alongside the `src/json` library tests.
- Full `ctest` run green.

### 6.2 Integration tests (`test/integration`, Python)

1. Full suite against the rebuilt release build:
   - Linux/CI: `test/integration/run-linux.sh`
   - direct: `python3 test/integration/test_runner.py --config release`
   - **No** `--skip-passed`/`QAPLA_IT_SKIP_PASSED` for the final run.
2. Adapt tests that assert on exact JSON text (candidates:
   `test/integration/mcp/mcp_tests.py`, `test/integration/mcp_settings/`,
   tournament/sprt tests reading status JSON): parse with Python's `json`
   module and assert on structure/values, not raw strings.
3. Extend where behavior changes:
   - malformed JSON request → server responds -32700 and keeps running,
   - tournament with an engine name containing special characters → rating
     table output passes `json.loads`,
   - capability cache written by the previous version is read correctly
     (fixture file in old format).
4. Final run recorded completely in `test/integration/test_results.log`.

**Exit criterion:** integration suite fully executed and green; adapted/new
tests committed.

## 7. Acceptance criteria

- [ ] `src/json` builds in `qapla-engine-tester` and `qapla-lib`;
      `src/json/test` runs in `qapla-unit-tests` only
- [ ] `json-helper.{h,cpp}` deleted; sweeps in 1.5 clean
- [ ] All interfaces from 1.4 pass `JsonValue`; no internal serialize→parse
      round trip remains
- [ ] Touched functions are shorter and simpler than before (2.2 yardstick) —
      reviewed per module, not just "compiles"
- [ ] `tournament-result`, `engine-capability` no longer hand-build/parse JSON;
      old persisted capability format still parses (unit + integration)
- [ ] MCP server survives malformed JSON with a proper JSON-RPC error
- [ ] Wire-format pin test (integral doubles, escaping) in place
- [ ] Full `ctest` green; full integration suite executed, adapted/extended as
      needed, all green
- [ ] `CHANGELOG.md` entry (compact serializer output; escaping fixed;
      internal interfaces now tree-typed)

## 8. Known intentional behavior changes

| Change | Where visible | Handling |
|---|---|---|
| Correct string escaping (previously raw `\t`, control chars, unescaped quotes in tournament JSON) | MCP payloads, rating table | New behavior is correct; integration asserts move to `json.loads` |
| Compact serializer output (no spaces after `:`/`,`) | persisted capability lines, MCP wire | Read compatibility tested both directions; CHANGELOG entry |
| Strict parsing at input boundaries (empty/trailing/malformed input rejected) | MCP stdin, capability lines | `try_parse` + defined error responses (2.3); integration test for -32700 |

## 9. Risks

| Risk | Mitigation |
|---|---|
| Sonnet performs a mechanical 1:1 translation without simplification | Guiding principle + 2.2 idiom table + per-step redo rule + acceptance criterion "shorter and simpler, reviewed" |
| Integral doubles serialize as `42.0` and break MCP clients/tests | Wire-format pin test in Phase 1, before any consumer changes |
| Strict parser rejects input the lenient old parser accepted | Parse-site audit (2.3); after 1.4 only real boundaries parse; integration suite as backstop |
| Persisted capability cache unreadable after migration | Old-format fixtures in unit + integration tests (5.3, 6.2) |
| Interface rebuild (1.4) ripples further than listed | Rebuild rule "serialize once, parse once" decides new cases; surface anything structural beyond that instead of improvising |
| Catch2 test glob breaks main build | CMake filter in Phase 0, verified by building both targets |
