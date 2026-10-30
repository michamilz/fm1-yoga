#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Serve the browser emulator (app/build/emu), or another folder, without caching: a fresh build is what the page
loads.
    python3 tools/serve-emu.py [PORT] [FOLDER]   (default 8792, app/build/emu; the local site: 8794 app/build/site)"""
import functools, http.server, os, sys


class NoCache(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        super().end_headers()


base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
root = sys.argv[2] if len(sys.argv) > 2 else os.path.join(base, "app", "build", "emu")
port = int(sys.argv[1]) if len(sys.argv) > 1 else 8792
http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(NoCache, directory=root)).serve_forever()
