#!/usr/bin/env python3
"""Census of the .sb2 script/dialogue files of an installed game: commands, argument counts, records.
Reads the player's own files; prints only counts and command names.

  python3 -I sb2census.py <game>/scripts
"""
import sys, os, struct, collections
root = sys.argv[1]
cmds = collections.Counter(); files = collections.Counter(); bad = []
argc = collections.defaultdict(set)
for dp, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith('.sb2'): continue
        p = os.path.join(dp, f); d = open(p, 'rb').read()
        try:
            n, = struct.unpack_from('<H', d, 0); i = 2
            for _ in range(n):
                l, = struct.unpack_from('<H', d, i); i += 2
                t = bytes(b ^ 0x30 for b in d[i:i+l]).decode('latin1'); i += l
                w = t.rstrip('\n').split('\t')
                cmds[w[0]] += 1; files[os.path.relpath(dp, root).split('/')[0]] += 1
                argc[w[0]].add(len(w) - 1)
            if i != len(d): bad.append((p, i, len(d)))
        except Exception as e:
            bad.append((p, str(e)))
print('records by folder', dict(files)); print('bad', len(bad), bad[:5])
print(len(cmds), 'distinct first words')
for c, k in cmds.most_common():
    print(f'{k:6d} {c!r} args={sorted(argc[c])}')
