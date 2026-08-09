"""
kntkta_ai/mcp/server.py — MCP JSON-RPC server implementation.

Implements the MCP protocol:
  - initialize / initialized
  - tools/list
  - tools/call
"""
import json
from .tool_registry import TOOL_REGISTRY


class KntktaMcpServer:
    """Minimal MCP server exposing KNTKTA tools."""

    SERVER_INFO = {
        "name": "kntkta-ai",
        "version": "1.0.0",
    }

    PROTOCOL_VERSION = "2024-11-05"

    async def handle_request(self, request: dict) -> dict:
        method = request.get("method", "")
        req_id = request.get("id")
        params = request.get("params", {})

        try:
            if method == "initialize":
                result = self._initialize(params)
            elif method == "initialized":
                return {}  # notification, no response
            elif method == "tools/list":
                result = self._list_tools()
            elif method == "tools/call":
                result = await self._call_tool(params)
            else:
                return self._error(req_id, -32601, f"Method not found: {method}")
        except Exception as e:
            return self._error(req_id, -32603, str(e))

        return {"jsonrpc": "2.0", "id": req_id, "result": result}

    def _initialize(self, params: dict) -> dict:
        return {
            "protocolVersion": self.PROTOCOL_VERSION,
            "capabilities": {"tools": {}},
            "serverInfo": self.SERVER_INFO,
        }

    def _list_tools(self) -> dict:
        tools = []
        for name, tool in TOOL_REGISTRY.items():
            tools.append({
                "name": name,
                "description": tool["description"],
                "inputSchema": tool["schema"],
            })
        return {"tools": tools}

    async def _call_tool(self, params: dict) -> dict:
        tool_name = params.get("name")
        arguments = params.get("arguments", {})

        if tool_name not in TOOL_REGISTRY:
            raise ValueError(f"Unknown tool: {tool_name}")

        handler = TOOL_REGISTRY[tool_name]["handler"]
        result = await handler(arguments)
        return {
            "content": [{"type": "text", "text": json.dumps(result)}],
            "isError": False,
        }

    @staticmethod
    def _error(req_id, code: int, message: str) -> dict:
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "error": {"code": code, "message": message},
        }
