#!/usr/bin/env python3
"""OMT.dll export table grouped per class, and which of them sb2.exe imports.

  python3 -I omt_exports.py OMT.dll sb2.exe exports.json
"""
import sys, json, pefile, re, collections
dll = pefile.PE(sys.argv[1]); exe = pefile.PE(sys.argv[2]); out = sys.argv[3]
exps = [(e.name.decode(), dll.OPTIONAL_HEADER.ImageBase + e.address) for e in dll.DIRECTORY_ENTRY_EXPORT.symbols if e.name]
def cls(n):
    m = re.match(r'\?+[0-9A-Z_]*?([A-Za-z_][A-Za-z0-9_]*)@([A-Za-z_][A-Za-z0-9_]*)@@', n)
    if n.startswith(('??0', '??1', '??_7', '??4', '??_G', '??_E', '??_F')):
        m2 = re.match(r'\?\?_?[0-9A-Z]([A-Za-z_][A-Za-z0-9_]*)@@', n)
        return m2.group(1) if m2 else None
    if m and n.startswith('??'):  # operator
        m3 = re.match(r'\?\?[0-9A-Z_]+?([A-Za-z_][A-Za-z0-9_]*)@@', n); return m3.group(1) if m3 else None
    m = re.match(r'\?[A-Za-z_][A-Za-z0-9_]*@([A-Za-z_][A-Za-z0-9_]*)@@', n)
    return m.group(1) if m else None
by = collections.defaultdict(list)
for n, a in exps: by[cls(n) or '<free>'].append((n, hex(a)))
imps = collections.defaultdict(list)
for e in exe.DIRECTORY_ENTRY_IMPORT:
    for i in e.imports:
        imps[e.dll.decode()].append(i.name.decode() if i.name else f'ord{i.ordinal}')
used = collections.Counter(cls(n) or '<free>' for n in imps.get('OMT.dll', []))
json.dump({'exports_by_class': by, 'sb2_imports': imps}, open(out, 'w'), indent=1)
print('exports', len(exps), 'classes', len([k for k in by if k != '<free>']))
print('sb2 imports per dll', {k: len(v) for k, v in imps.items()})
print('OMT classes used by sb2.exe:', len(used))
for k, v in used.most_common(): print(f'  {v:4d} {k}  (dll exports {len(by[k])})')
