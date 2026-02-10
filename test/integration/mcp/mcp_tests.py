#!/usr/bin/env python3
"""MCP Tests - Basic MCP protocol tests."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of MCP tests."""
    return [
        {
            "name": "mcp-initialize",
            "description": "Start MCP server, send initialize, and expect correct response",
            "args": "--mcp",
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
            ]
        },
        {
            "name": "mcp-list-engines",
            "description": "Start MCP server with engines and list them",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini",
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
            ]
        },
        {
            "name": "mcp-engine-details",
            "description": "Get details for a specific engine",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini",
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
                    "content": "Command: test/integration/engines/stockfish-windows-x86-64-avx2.exe",
                    "isRegex": False
                }
            ]
        },
        {
            "name": "mcp-engine-add",
            "description": "Add a new engine",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "add", "engine_name": "TestEngine", "engine_cmd": "echo.exe"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Engine 'TestEngine' added successfully.",
                    "isRegex": False
                }
            ]
        },
        {
            "name": "mcp-engine-copy",
            "description": "Copy an existing engine configuration",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "copy", "engine_name": "Stockfish", "engine_copyName": "StockfishCopy"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Engine 'Stockfish' copied to 'StockfishCopy'.",
                    "isRegex": False
                }
            ]
        },
        {
            "name": "mcp-engine-update",
            "description": "Update an existing engine configuration",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "manage_engines", "arguments": {"command": "update", "engine_name": "Stockfish", "engine_option_Hash": "128"}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": "Engine 'Stockfish' updated successfully.",
                    "isRegex": False
                }
            ]
        },
        {
            "name": "mcp-engine-update-all",
            "description": "Update all engines with common settings",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini",
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
            ]
        },
        {
            "name": "mcp-engine-error-missing-name",
            "description": "Try to get details without engine_name",
            "args": "--settingsfile=test/integration/mcp/mcp-engines.ini",
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
            ]
        }
    ]
