#!/usr/bin/env python3
"""Send one-line commands to a running OpenApoc harness (localhost TCP).

Launch the game with:
  --Framework.Harness.Enable=1 --Framework.Harness.Port=17321 --Game.SkipIntro=1 --Config.Save=0

Commands (see framework/harness.h):
  status
  click X Y [left|right|middle]
  move X Y
  down X Y [button]
  up X Y [button]
  scroll X Y DY [DX]
  key NAME
  keydown NAME
  keyup NAME
  text STRING
  screenshot PATH
  quit
  wait SECONDS          (client-side)
"""

from __future__ import annotations

import argparse
import socket
import sys
import time


def send(host: str, port: int, line: str, timeout: float) -> str:
    payload = line if line.endswith("\n") else line + "\n"
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.sendall(payload.encode("utf-8"))
        chunks: list[bytes] = []
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
            if b"\n" in data:
                break
    return b"".join(chunks).decode("utf-8", errors="replace").strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=17321)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("command", nargs=argparse.REMAINDER, help="harness command words")
    args = parser.parse_args()
    if not args.command:
        parser.error("missing command (try: status)")
    if args.command[0] == "--":
        args.command = args.command[1:]
    if args.command and args.command[0].lower() == "wait":
        seconds = float(args.command[1]) if len(args.command) > 1 else 1.0
        time.sleep(seconds)
        print("OK waited", seconds)
        return 0
    line = " ".join(args.command)
    try:
        reply = send(args.host, args.port, line, args.timeout)
    except OSError as exc:
        print(f"ERR connect {args.host}:{args.port}: {exc}", file=sys.stderr)
        return 2
    print(reply)
    return 0 if reply.startswith("OK") else 1


if __name__ == "__main__":
    raise SystemExit(main())
