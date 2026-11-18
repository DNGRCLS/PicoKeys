#!/usr/bin/env python3
"""Gzip an HTML file and emit it as a C header (const array in flash)."""
import gzip
import sys

src, dst = sys.argv[1], sys.argv[2]
with open(src, "rb") as f:
    raw = f.read()
gz = gzip.compress(raw, compresslevel=9, mtime=0)

lines = []
for i in range(0, len(gz), 20):
    lines.append("  " + ",".join(f"0x{b:02x}" for b in gz[i:i + 20]) + ",")

import os
os.makedirs(os.path.dirname(dst), exist_ok=True)
with open(dst, "w") as f:
    f.write("// Generated from web/index.html - do not edit\n")
    f.write("#pragma once\n#include <stdint.h>\n")
    f.write(f"// raw {len(raw)} bytes, gzip {len(gz)} bytes\n")
    f.write(f"static const uint32_t WEB_PAGE_GZ_LEN = {len(gz)};\n")
    f.write("static const uint8_t WEB_PAGE_GZ[] = {\n")
    f.write("\n".join(lines))
    f.write("\n};\n")
print(f"web page: {len(raw)} bytes -> {len(gz)} bytes gzip")
