#!/usr/bin/env python3
"""Raw-decompile every function of a PE (sb2.exe or OMT.dll) with r2ghidra.

  python3 decompile_pe.py <pe> <outdir> [exports.json]

Differences from neutron1's decompile_all.py: no RTTI (OMT.dll has none), so functions are named from
the export table and grouped per class by the mangled export name; and functions that `aaa` misses
are seeded from every direct call target / exported address in .text (the scene loader at 0x437414
in sb2.exe is one). Writes outdir/<Class>.c, outdir/_free.c and outdir/functions.json.
"""
import json, os, re, struct, sys
import pefile, r2pipe

pe_path, outdir = sys.argv[1:3]
os.makedirs(outdir, exist_ok=True)
pe = pefile.PE(pe_path)
base = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
t0, t1 = base + text.VirtualAddress, base + text.VirtualAddress + text.Misc_VirtualSize
code = text.get_data()

exports = {}
if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name and t0 <= base + e.address < t1:
            exports.setdefault(base + e.address, e.name.decode())

def owner_class(mangled):
    m = re.match(r'\?\?[0-9A-Z_]{1,2}([A-Za-z_]\w*)@@', mangled)       # ctor/dtor/operators/vftable
    if m: return m.group(1)
    m = re.match(r'\?[A-Za-z_]\w*@([A-Za-z_]\w*)@@', mangled)          # method
    return m.group(1) if m else None

# E8 rel32 call targets inside .text (cheap over-approximation; r2 rejects non-code later)
seeds = set(exports)
for i in range(len(code) - 5):
    if code[i] == 0xE8:
        tgt = t0 + i + 5 + struct.unpack_from('<i', code, i + 1)[0]
        if t0 <= tgt < t1: seeds.add(tgt)

r2 = r2pipe.open(pe_path, flags=['-2', '-e', 'scr.color=0', '-e', 'bin.relocs.apply=true'])
r2.cmd('e r2ghidra.indent=4; e r2ghidra.casts=true; e anal.hasnext=false; aaa')
have = {f['addr'] for f in (r2.cmdj('aflj') or [])}
added = 0
for a in sorted(seeds - have):
    # stray E8 bytes inside instructions: keep only targets that start like an MSVC 6 function
    b = code[a - t0:a - t0 + 3]
    if a not in exports and not (b[:3] == b'\x55\x8b\xec' or b[:1] in (b'\x83', b'\x81', b'\x56', b'\x53',
                                                                    b'\x57', b'\x6a', b'\x64', b'\x8b', b'\x51', b'\xb8', b'\xa1', b'\xd9')):
        continue
    r2.cmd(f'af @ {a}'); added += 1
# Gap fill: code reached only through function pointers (window procs, callbacks, jump tables in
# vtables of the DLL) is invisible to aaa. Start a function at the first non-padding byte of every
# uncovered stretch of .text, and repeat until nothing new appears.
tried = set()
for rnd in range(60):
    fl = sorted((f['addr'], f['addr'] + max(f.get('realsz', f.get('size', 0)), 1))
                for f in (r2.cmdj('aflj') or []))
    gaps, cur = [], t0
    for s, e in fl:
        if s > cur: gaps.append((cur, s))
        cur = max(cur, e)
    if cur < t1: gaps.append((cur, t1))
    new = 0
    for s, e in gaps:
        o = s - t0
        while o < e - t0 and code[o] in (0xCC, 0x90, 0x00): o += 1
        # MSVC aligns functions to 16; an "8d 49 00"/"8b ff" filler also shows up
        if o >= e - t0 or e - (t0 + o) < 3: continue
        a = t0 + o
        if a in tried: continue
        tried.add(a)
        r2.cmd(f'af @ {a}')
        if r2.cmd(f'afi. @ {a}').strip(): new += 1
    added += new
    print(f'gap round {rnd}: {len(gaps)} gaps, {new} new', flush=True)
    if not new: break
for a, n in exports.items():
    r2.cmd(f'afn x_{re.sub(r"[^A-Za-z0-9_]", "_", n)[:180]} @ {a}')
funcs = sorted(r2.cmdj('aflj') or [], key=lambda f: f['addr'])
print(f'{len(funcs)} functions ({added} seeded beyond aaa)', flush=True)

files, index = {}, {}
for i, f in enumerate(funcs):
    a = f['addr']
    exp = exports.get(a)
    cls = owner_class(exp) if exp else None
    try: body = r2.cmd(f'pdg @ {a}')
    except Exception as e: body = f'/* decompile failed: {e} */\n'
    key = cls or '_free'
    fh = files.get(key) or files.setdefault(key, open(os.path.join(outdir, key + '.c'), 'w'))
    fh.write(f'\n/* ==== {hex(a)}  {exp or f["name"]}  size {f.get("size", 0)} ==== */\n{body}\n')
    index[hex(a)] = dict(name=f['name'], export=exp, cls=cls, size=f.get('size', 0))
    if i % 200 == 0: print(f'{i}/{len(funcs)}', flush=True)
for fh in files.values(): fh.close()
json.dump(index, open(os.path.join(outdir, 'functions.json'), 'w'), indent=0)
print('done', flush=True)
