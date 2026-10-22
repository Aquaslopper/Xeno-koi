#!/usr/bin/env python3
"""Serve the POM preview.

The prototype reads the game's real art (`assets/*.jpg`, `assets/*_normal.png`,
`assets/height/*_height.png`), so the document root has to be the repository root;
this puts a redirect on `/` so the preview opens on the page itself.

    python3 prototype/pom/serve.py            # http://localhost:8000/prototype/pom/
    python3 prototype/pom/serve.py --port 9000
"""
import argparse
import functools
import http.server
import os
import socketserver
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
URL = "/" + os.path.relpath(HERE, REPO).replace(os.sep, "/") + "/"


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".frag": "text/plain",
        ".js": "text/javascript",
    }

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self.send_response(302)
            self.send_header("Location", URL)
            self.end_headers()
            return
        super().do_GET()

    def log_message(self, fmt, *args):
        sys.stderr.write("%s\n" % (fmt % args))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--bind", default="0.0.0.0")
    args = ap.parse_args()
    handler = functools.partial(Handler, directory=REPO)
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer((args.bind, args.port), handler) as httpd:
        print(f"POM preview: http://localhost:{args.port}{URL}  (root: {REPO})")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
