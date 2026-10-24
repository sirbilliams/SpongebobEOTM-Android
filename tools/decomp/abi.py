#!/usr/bin/env python3
"""Exact calling convention facts for every function, straight from the machine code.

The decompiler's prototypes are guesses; the binary is not. For each function in functions.json:
  ret_pop   bytes popped by `ret n` (callee-cleaned: __thiscall/__stdcall) -> n/4 stack args
  uses_ecx  ecx read before written (this pointer / fastcall arg 1)
  uses_edx  edx read before written (fastcall arg 2)
  fret      ends with an x87 value on the stack (float/double return)

    abi.py Neutron2.exe raw/functions.json out.json
"""
import json, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_REG, X86_REG_ECX, X86_REG_EDX, X86_REG_CX, X86_REG_CL, X86_REG_DX, X86_REG_DL

ECX = {X86_REG_ECX, X86_REG_CX, X86_REG_CL}
EDX = {X86_REG_EDX, X86_REG_DX, X86_REG_DL}


def main():
    exe, fj, out = sys.argv[1:4]
    pe = pefile.PE(exe)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
    funcs = json.load(open(fj))
    res = {}
    for a, f in funcs.items():
        start = int(a, 16); size = f.get('size') or 0
        if size <= 0 or size > 200000:
            continue
        code = image[start - base:start - base + size]
        pops = set(); seen_w = set(); r_ecx = r_edx = False; last = None; fpu_load = False
        for ins in md.disasm(code, start):
            regs_r, regs_w = ins.regs_access()
            rr = set(regs_r); ww = set(regs_w)
            # xor ecx,ecx style zeroing reads ecx formally but is a write
            if ins.mnemonic in ('xor', 'sub') and len(ins.operands) == 2 and ins.operands[0].type == X86_OP_REG \
               and ins.operands[1].type == X86_OP_REG and ins.operands[0].reg == ins.operands[1].reg:
                rr.discard(ins.operands[0].reg)
            if not r_ecx and 'ecx' not in seen_w and rr & ECX: r_ecx = True
            if not r_edx and 'edx' not in seen_w and rr & EDX: r_edx = True
            if ww & ECX: seen_w.add('ecx')
            if ww & EDX: seen_w.add('edx')
            if ins.mnemonic == 'ret':
                pops.add(ins.operands[0].imm if ins.operands else 0)
                fpu_load = last is not None and last.mnemonic.startswith('f') and last.mnemonic not in ('fstp', 'fistp', 'fst')
            if ins.mnemonic in ('call',) and 'ecx' not in seen_w:
                seen_w.add('ecx'); seen_w.add('edx')      # clobbered by the callee
            last = ins
        res[a] = dict(ret_pop=sorted(pops), uses_ecx=r_ecx, uses_edx=r_edx, fret=fpu_load)
    json.dump(res, open(out, 'w'))
    print(f'{len(res)} functions')


if __name__ == '__main__':
    main()
