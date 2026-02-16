#!/usr/bin/env python3
"""MCP Settings Tests - Tests for the list_settings tool."""

from typing import List, Dict, Any


def get_tests() -> List[Dict[str, Any]]:
    """Return list of MCP settings tests."""
    return [
        {
            "name": "mcp-list-settings",
            "description": "Call list_settings tool and verify output format",
            "args": "--mcp",
            "log_path": "test/integration/log/mcp_settings/list",
            "input": '{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "list_settings", "arguments": {}}}',
            "validators": [
                {"type": "exitCode", "expected": 0},
                {
                    "type": "stdout",
                    "content": '"result":{',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": '"type":"text"',
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Global Settings",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Settings Group: openings",
                    "isRegex": False
                },
                {
                    "type": "stdout",
                    "content": "Settings Group: epd",
                    "isRegex": False
                }
            ],
            "cleanup": "test/integration/log/mcp_settings/list",
        }
    ]
