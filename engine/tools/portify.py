#!/usr/bin/env python3
"""Make Win32/MSVC-era OMT client code (samples, decompiled game code) build on 64-bit Linux/Android.

  - `long` -> `int` (OMT and its files assume 32-bit long; `long long`/`long double` kept)
  - `%ld/%lu/%lx` -> `%d/%u/%x`
  - `#include "X.h"` fixed to the real filename case found in the include dirs
  - CRLF -> LF

usage: portify.py <out_dir> <include_dir_root> <files...>
"""
import os, re, sys

def build_header_index(root):
    idx = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            if f.lower().endswith(('.h', '.hpp')):
                idx.setdefault(f.lower(), f)
    return idx

def portify(src, hdr):
    s = src.replace('\r\n', '\n').replace('\r', '\n')
    s = re.sub(r'\blong\s+long\b', '__OMT_LL__', s)
    s = re.sub(r'\blong\s+double\b', '__OMT_LD__', s)
    s = re.sub(r'\blong\b', 'int', s)
    s = s.replace('__OMT_LL__', 'long long').replace('__OMT_LD__', 'long double')
    s = re.sub(r'%l([dux])', r'%\1', s)
    def fix_inc(m):
        name = m.group(2)
        base = os.path.basename(name)
        real = hdr.get(base.lower())
        if real and real != base:
            name = name[: len(name) - len(base)] + real
        return f'{m.group(1)}"{name}"'
    s = re.sub(r'(#\s*include\s*)"([^"]+)"', fix_inc, s)
    return s

if __name__ == '__main__':
    out, inc_root, files = sys.argv[1], sys.argv[2], sys.argv[3:]
    os.makedirs(out, exist_ok=True)
    hdr = build_header_index(inc_root)
    for f in files:
        dst = os.path.join(out, os.path.basename(f))
        open(dst, 'w', encoding='latin1').write(portify(open(f, encoding='latin1').read(), hdr))
        print('portified', dst)
