#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Serve the browser emulator (app/build/emu) without caching: a fresh build is what the page loads.
    python3 tools/serve-emu.py [PORT]   (default 8792)"""
import functools, http.server, os, sys


class NoCache(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        super().end_headers()


root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "app", "build", "emu")
port = int(sys.argv[1]) if len(sys.argv) > 1 else 8792
http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(NoCache, directory=root)).serve_forever()
