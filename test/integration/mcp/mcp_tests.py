#!/usr/bin/env python3
"""MCP Tests - Basic MCP protocol tests."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of MCP tests."""
    return [
        {
            "name": "mcp-initialize",
            "description": "Start MCP server, send initialize, and expect correct response",
            "args": "--mcp --logging path=test/integration/log/mcp/initialize",
            "log_path": "test/integration/log/mcp/initialize",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2024-11-05", "capabilities": {}, "serverInfo": {"name": "test-client", "version": "1.0"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": '"result":{',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": '"protocolVersion":"2024-11-05"',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": '"name":"Qapla Engine Tester"',
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/initialize",
        },
        {
            "name": "mcp-list-engines",
            "description": "Start MCP server with engines and list them",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/list",
            "log_path": "test/integration/log/mcp/list",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "list"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Registered Engines:",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "- Stockfish",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "- Qapla 0.4.0",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/list",
        },
        {
            "name": "mcp-engine-details",
            "description": "Get details for a specific engine",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/details",
            "log_path": "test/integration/log/mcp/details",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "details", "engine_name": "Stockfish"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Details for 'Stockfish':",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "stockfish-windows-x86-64-avx2.exe",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Protocol: uci",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/details",
        },
        {
            "name": "mcp-engine-add",
            "description": "Add a new engine",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/add",
            "log_path": "test/integration/log/mcp/add",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "add", "engine_name": "TestEngine", "engine_cmd": "test/engines/Qapla0.4.0.exe"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Engine 'TestEngine' added successfully.",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/add",
        },
        {
            "name": "mcp-engine-copy-basic",
            "description": "Copy an existing engine configuration",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/copy",
            "log_path": "test/integration/log/mcp/copy",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "copy", "engine_name": "Stockfish", "engine_copyName": "StockfishCopy"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Engine 'Stockfish' copied to 'StockfishCopy'.",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/copy",
        },
        {
            "name": "mcp-engine-update",
            "description": "Update an existing engine configuration",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/update",
            "log_path": "test/integration/log/mcp/update",
            "input": [
                '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "update", "engine_name": "Stockfish", "engine_tc": "3+0.02"}}}',
                '{"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "details", "engine_name": "Stockfish"}}}'
            ],
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Engine 'Stockfish' updated successfully.",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Time Control: 3.0+0.02",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/update",
        },
        {
            "name": "mcp-engine-update-all",
            "description": "Update all engines with common settings",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/update-all",
            "log_path": "test/integration/log/mcp/update-all",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "update_all", "engine_option_Threads": "2"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Updated",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "engines.",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/update-all",
        },
        {
            "name": "mcp-engine-error-missing-name",
            "description": "Try to get details without engine_name",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/error",
            "log_path": "test/integration/log/mcp/error",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "details"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": '"isError":true',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Command 'details' requires 'engine_name'.",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/error",
        },
        {
            "name": "mcp-sprt-start",
            "description": "Verify that an SPRT test can be started and engine activation works",
            "args": "--settingsfile=test/integration/mcp/mcp-sprt-test.ini --logging path=test/integration/log/mcp/sprt",
            "log_path": "test/integration/log/mcp/sprt",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "sprt", "arguments": {"engines": "Stockfish,Qapla 0.4.0", "sprt_maxgames": 1, "engine_tc": "40/1+0.1"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": '"isError":false',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Tool 'sprt' finished. Result: ",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/sprt",
        },
        {
            "name": "mcp-sprt-missing-tc",
            "description": "Call sprt with missing TC and expect an error message but no process exit code error. Further check that server is still alive.",
            "args": "--settingsfile=test/integration/mcp/mcp-sprt-test.ini --logging path=test/integration/log/mcp/sprt-missing-tc",
            "log_path": "test/integration/log/mcp/sprt-missing-tc",
            "input": [
                '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "sprt", "arguments": {"engines": "Stockfish,Qapla 0.4.0", "sprt_maxgames": 1}}}',
                '{"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "list"}}}'
            ],
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": '"id":1',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "No valid time control defined",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "engine_tc",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": '"id":2',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Registered Engines:",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/sprt-missing-tc",
        },
        {
            "name": "mcp-engine-delete",
            "description": "Add an engine and then delete it",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/delete",
            "log_path": "test/integration/log/mcp/delete",
            "input": [
                '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "add", "engine_name": "ToDeepDelete", "engine_cmd": "test/integration/engines/stockfish-windows-x86-64-avx2.exe"}}}',
                '{"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "delete", "engine_name": "ToDeepDelete"}}}',
                '{"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "list"}}}'
            ],
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Engine 'ToDeepDelete' added successfully", "isRegex": False},
                {"type": "stdout", "content": "Engine 'ToDeepDelete' deleted successfully", "isRegex": False}
            ],
            "cleanup": "test/integration/log/mcp/delete",
        },
        {
            "name": "mcp-engine-copy-with-tc",
            "description": "Copy an engine and verify that settings like TC are preserved",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/copy-verify",
            "log_path": "test/integration/log/mcp/copy-verify",
            "input": [
                '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "update", "engine_name": "Stockfish", "engine_tc": "5+0.05"}}}',
                '{"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "copy", "engine_name": "Stockfish", "engine_copyName": "Stockfish-Copy"}}}',
                '{"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "details", "engine_name": "Stockfish-Copy"}}}'
            ],
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Engine 'Stockfish' updated successfully", "isRegex": False},
                {"type": "stdout", "content": "Engine 'Stockfish' copied to 'Stockfish-Copy'", "isRegex": False},
                {"type": "stdout", "content": "Time Control: 5.0+0.05", "isRegex": False}
            ],
            "cleanup": "test/integration/log/mcp/copy-verify",
        },
        {
            "name": "mcp-engine-copy-inline",
            "description": "Copy an engine and set a UCI option simultaneously",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/copy-inline",
            "log_path": "test/integration/log/mcp/copy-inline",
            "input": [
                '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "copy", "engine_name": "Stockfish", "engine_copyName": "Stockfish-64", "engine_option_Hash": "64"}}}',
                '{"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "details", "engine_name": "Stockfish-64"}}}'
            ],
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Engine 'Stockfish' copied to 'Stockfish-64'", "isRegex": False},
                {"type": "stdout", "content": "hash = 64", "isRegex": False}
            ],
            "cleanup": "test/integration/log/mcp/copy-inline",
        },
        {
            "name": "mcp-engine-invalid-path",
            "description": "Try to add an engine with a non-existent path",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/invalid-path",
            "log_path": "test/integration/log/mcp/invalid-path",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "add", "engine_name": "NoExist", "engine_cmd": "this/path/does/not/exist.exe"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Engine executable not found at path", "isRegex": False},
                {"type": "stdout", "content": '"isError":true', "isRegex": False}
            ],
            "cleanup": "test/integration/log/mcp/invalid-path",
        },
        {
            "name": "mcp-engine-update-all-custom",
            "description": "Verify that update_all also affects dynamically added/copied engines",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini --logging path=test/integration/log/mcp/update-all-custom",
            "log_path": "test/integration/log/mcp/update-all-custom",
            "input": [
                '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "copy", "engine_name": "Stockfish", "engine_copyName": "CustomCopy"}}}',
                '{"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "update_all", "engine_option_Hash": "256"}}}',
                '{"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "details", "engine_name": "CustomCopy"}}}'
            ],
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Updated 4 engines", "isRegex": False},
                {"type": "stdout", "content": "hash = 256", "isRegex": False}
            ],
            "cleanup": "test/integration/log/mcp/update-all-custom",
        },
        {
            "name": "mcp-engine-update-all-after-sprt",
            "description": "Verify that update_all still works after engines have been used in a tournament",
            "args": "--settingsfile=test/integration/mcp/mcp-sprt-test.ini --logging path=test/integration/log/mcp/update-all-after-sprt",
            "log_path": "test/integration/log/mcp/update-all-after-sprt",
            "input": [
                '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "copy", "engine_name": "Stockfish", "engine_copyName": "Stockfish-Copy"}}}',
                '{"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "sprt", "arguments": {"engines": "Stockfish,Stockfish-Copy", "sprt_maxgames": 1, "engine_tc": "3+0.01"}}}',
                '{"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "update_all", "engine_tc": "10/10+1"}}}',
                '{"jsonrpc": "2.0", "id": 4, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "details", "engine_name": "Stockfish-Copy"}}}'
            ],
            "validators": [
                {"type": "exitCode", "expected": 0},
                {"type": "stdout", "content": "Time Control: 10/10.0+1.00", "isRegex": False}
            ],
            "cleanup": "test/integration/log/mcp/update-all-after-sprt",
        }
    ]
