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
                    "content": "- Qapla 0.3.0",
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
            "name": "mcp-engine-copy",
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
                    "content": "tc: 3.0+0.02",
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
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "sprt", "arguments": {"engines": "Stockfish,Qapla 0.3.0", "sprt_maxgames": 1, "engine_tc": "40/1+0.1"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": '"isError":false',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Result of SPRT between Stockfish and Qapla 0.3.0",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp/sprt",
        }
    ]
