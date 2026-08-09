"""
kntkta_ai/server.py — MCP AI server entry point.

Runs a local MCP server that exposes KNTKTA tools to a local LLM.
The LLM (via Ollama) can call these tools to control the controller surface,
presets, sequencer, and sample engine.

Usage:
  python server.py               # stdio mode (default)
  python server.py --tcp 8765    # TCP socket mode
  python server.py --voice       # enable voice input via Whisper
"""
import argparse
import asyncio
import json
import sys
from .mcp.server import KntktaMcpServer
from .mcp.llm_bridge import LlmBridge


async def run_stdio(server: KntktaMcpServer):
    """Run MCP over stdin/stdout (for subprocess integration)."""
    loop = asyncio.get_event_loop()
    reader = asyncio.StreamReader()
    protocol = asyncio.StreamReaderProtocol(reader)
    await loop.connect_read_pipe(lambda: protocol, sys.stdin.buffer)

    while True:
        line = await reader.readline()
        if not line:
            break
        try:
            request = json.loads(line.decode())
            response = await server.handle_request(request)
            sys.stdout.buffer.write((json.dumps(response) + "\n").encode())
            sys.stdout.buffer.flush()
        except json.JSONDecodeError:
            pass
        except Exception as e:
            error_resp = {"jsonrpc": "2.0", "error": {"code": -32603, "message": str(e)}}
            sys.stdout.buffer.write((json.dumps(error_resp) + "\n").encode())
            sys.stdout.buffer.flush()


async def run_tcp(server: KntktaMcpServer, port: int):
    """Run MCP over TCP socket."""
    async def handle_client(reader, writer):
        while True:
            line = await reader.readline()
            if not line:
                break
            try:
                request = json.loads(line.decode())
                response = await server.handle_request(request)
                writer.write((json.dumps(response) + "\n").encode())
                await writer.drain()
            except Exception as e:
                error_resp = {"jsonrpc": "2.0", "error": {"code": -32603, "message": str(e)}}
                writer.write((json.dumps(error_resp) + "\n").encode())
                await writer.drain()
        writer.close()

    tcp_server = await asyncio.start_server(handle_client, "127.0.0.1", port)
    print(f"[kntkta_ai] MCP server listening on 127.0.0.1:{port}", file=sys.stderr)
    async with tcp_server:
        await tcp_server.serve_forever()


def main():
    parser = argparse.ArgumentParser(description="KNTKTA MCP AI Server")
    parser.add_argument("--tcp", type=int, default=0, metavar="PORT",
                        help="Run in TCP mode on given port (default: stdio)")
    parser.add_argument("--voice", action="store_true",
                        help="Enable voice input via Whisper")
    parser.add_argument("--model", default="mistral",
                        help="Ollama model name (default: mistral)")
    args = parser.parse_args()

    server = KntktaMcpServer()
    bridge = LlmBridge(model=args.model, voice=args.voice)

    if args.tcp:
        asyncio.run(run_tcp(server, args.tcp))
    else:
        asyncio.run(run_stdio(server))


if __name__ == "__main__":
    main()
