#!/usr/bin/env python3
"""Minimal HTTP server returning network_hmi_example.json for ui_network login test."""

import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent
JSON_PATH = ROOT / "network_hmi_example.json"


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = JSON_PATH.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print(fmt % args)


def main():
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    print("Serving HMI JSON on http://0.0.0.0:8080/")
    print("Device URL example: http://<PC_IP>:8080/")
    print("Payload preview:", data.get("title"))
    HTTPServer(("0.0.0.0", 8080), Handler).serve_forever()


if __name__ == "__main__":
    main()
