#!/usr/bin/env python3
"""Inline the wasm engine into the web app.

usage: bundle.py <src/index.html> <p10.wasm> <out/index.html>
Writes two files: <out> is a complete standalone page (open it from a local
web server), and <out dir>/artifact.html is the same page as a body fragment
for hosts that supply their own document skeleton.
"""
import base64
import os
import sys

src, wasm, out = sys.argv[1:4]
page = open(src, encoding="utf-8").read()
b64 = base64.b64encode(open(wasm, "rb").read()).decode("ascii")
if "__P10_WASM_BASE64__" not in page:
    raise SystemExit("placeholder missing in " + src)
page = page.replace("__P10_WASM_BASE64__", b64)
head_end = page.index('<div class="instrument"')
standalone = ("<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
              "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, viewport-fit=cover\">\n"
              + page[:head_end] + "</head>\n<body>\n" + page[head_end:] + "\n</body>\n</html>\n")
open(out, "w", encoding="utf-8").write(standalone)
open(os.path.join(os.path.dirname(out), "artifact.html"), "w", encoding="utf-8").write(page)
print(f"{out}: {len(standalone) / 1024:.0f} KB")
