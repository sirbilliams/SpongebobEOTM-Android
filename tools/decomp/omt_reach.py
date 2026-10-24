#!/usr/bin/env python3
"""Which OMT.dll code can sb2.exe reach? Start at the exports sb2.exe imports, follow direct calls and
jumps, and when a function stores a vtable address (mov [reg], imm32 of a ??_7 export), follow every
method in that vtable. Result per class: reached exports / all exports, plus total reached code.

  python3 -I omt_reach.py OMT.dll exports.json reach.json
"""
import json, re, struct, sys
import pefile, capstone

dll, exj, out = sys.argv[1:4]
pe = pefile.PE(dll); base = pe.OPTIONAL_HEADER.ImageBase; img = pe.get_memory_mapped_image()
text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
t0, t1 = base + text.VirtualAddress, base + text.VirtualAddress + text.Misc_VirtualSize
ex = json.load(open(exj))
name_at, addr_of = {}, {}
for cls, lst in ex['exports_by_class'].items():
    for n, a in lst:
        name_at[int(a, 16)] = (n, cls); addr_of[n] = int(a, 16)
vtables = {a: n for a, (n, c) in name_at.items() if n.startswith('??_7')}

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = False
def u32(va): return struct.unpack_from('<I', img, va - base)[0]

def vt_methods(va):
    out = []
    while True:
        p = u32(va)
        if not (t0 <= p < t1): break
        out.append(p); va += 4
        if va in vtables or va in name_at: break
    return out

seen, work, size = set(), [], {}
for n in ex['sb2_imports'].get('OMT.dll', []):
    a = addr_of.get(n)
    if a: work.append(a)
while work:
    f = work.pop()
    if f in seen or not (t0 <= f < t1): continue
    seen.add(f)
    # linear sweep to the first ret that isn't jumped over (good enough for MSVC 6 code)
    va, far, n = f, f, 0
    for ins in md.disasm(img[f - base:f - base + 0x8000], f):
        n += 1
        ops = ins.op_str
        if ins.mnemonic in ('call', 'jmp') or ins.mnemonic.startswith('j'):
            m = re.fullmatch(r'0x([0-9a-f]+)', ops)
            if m:
                tgt = int(m.group(1), 16)
                if ins.mnemonic == 'call' or not (f <= tgt < f + 0x8000) : work.append(tgt)
                elif tgt > far: far = tgt
        for m in re.finditer(r'0x([0-9a-f]{8})', ops):
            v = int(m.group(1), 16)
            if v in vtables: work.extend(vt_methods(v))
        if ins.mnemonic == 'ret' and ins.address >= far: va = ins.address + ins.size; break
        if ins.mnemonic == 'jmp' and ins.address >= far and not re.fullmatch(r'0x[0-9a-f]+', ops):
            va = ins.address + ins.size; break
        va = ins.address + ins.size
    size[f] = va - f

per = {}
for cls, lst in ex['exports_by_class'].items():
    hit = [n for n, a in lst if int(a, 16) in seen]
    per[cls] = (len(hit), len(lst))
json.dump({'reached': {hex(a): size[a] for a in sorted(seen)}, 'per_class': per}, open(out, 'w'), indent=0)
print(f'reached {len(seen)} functions, {sum(size.values())} bytes of {t1 - t0} .text')
reached_cls = sorted((c for c, (h, t) in per.items() if h and c != '<free>'), key=lambda c: -per[c][0])
print(f'classes with reached exports: {len(reached_cls)}')
print(' '.join(f'{c}({per[c][0]}/{per[c][1]})' for c in reached_cls))
print('never reached:', ' '.join(c for c, (h, t) in sorted(per.items()) if not h))
