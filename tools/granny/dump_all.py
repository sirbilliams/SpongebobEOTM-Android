#!/usr/bin/env python3
"""Group the game's .grn files by model and dump each group with grndump.exe under Wine.

  python3 dump_all.py <game>/Program_Executable_Files/grn <outdir> [grndump.exe] [granny.dll]

Animations are matched to the model whose '<name>base.grn' is the longest case-insensitive
prefix of the animation's file name (jimmyfullrungun.grn -> jimmybase.grn). Files that match
no base are dumped as standalone models. Writes <outdir>/<model>.gdmp and <outdir>/report.json.
"""
import os, sys, json, subprocess, shutil

grn_dir, out_dir = sys.argv[1], sys.argv[2]
tool = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(__file__), 'grndump.exe')
dll = sys.argv[4] if len(sys.argv) > 4 else os.path.join(grn_dir, '..', 'granny.dll')
os.makedirs(out_dir, exist_ok=True)
for f in (tool, dll):
    shutil.copy(f, out_dir)

files = sorted(f for f in os.listdir(grn_dir) if f.lower().endswith('.grn'))
bases = {f[:-len('base.grn')].lower(): f for f in files if f.lower().endswith('base.grn')}
bases.update({f[:-len('base2.grn')].lower() + '#2': f for f in files if f.lower().endswith('base2.grn')})
groups = {b: [] for b in bases.values()}
standalone = []
# Animations whose names don't share their model's prefix (decided by name; Granny binds by bone
# name so it will happily play them on several skeletons). gunbones.grn is left unassigned.
ALIASES = {'nummyflail.grn': 'nummeybase.grn', 'numflyair.grn': 'nummeyflyerbase.grn',
           'upperrun.grn': 'jimmybase.grn'}
for f in files:
    if f in groups:
        continue
    low = f.lower()
    alias = ALIASES.get(low) or ('nummeybase.grn' if low.startswith('nummy') else None)
    target = next((b for b in groups if b.lower() == alias), None) if alias else None
    if target:
        groups[target].append(f); continue
    best = max((p for p in bases if not p.endswith('#2') and low.startswith(p)), key=len, default=None)
    if best: groups[bases[best]].append(f)
    else: standalone.append(f)

env = dict(os.environ, WINEDEBUG='-all', WINEARCH='win32', WINEPREFIX=os.environ.get('WINEPREFIX', '/root/.wine32'))
report = {}
def run(name, model, anims):
    out = name + '.gdmp'
    args = ['wine', 'grndump.exe', out, 'Z:' + os.path.join(os.path.abspath(grn_dir), model)] + \
           ['Z:' + os.path.join(os.path.abspath(grn_dir), a) for a in anims]
    try:
        r = subprocess.run(args, cwd=out_dir, env=env, capture_output=True, text=True, timeout=900)
        log = (r.stderr or '') + (r.stdout or '')
        ok = r.returncode == 0 and os.path.exists(os.path.join(out_dir, out))
    except subprocess.TimeoutExpired:
        log, ok = 'timeout', False
    failed = [l.split()[1] for l in log.splitlines() if l.startswith('animation ') and 'failed' in l]
    report[name] = dict(model=model, animations=len(anims), failed_animations=failed, ok=ok,
                        log=log.strip().splitlines()[-1] if log.strip() else '')
    print(f"{'ok ' if ok else 'ERR'} {name:<24} {len(anims):3d} anims  {report[name]['log']}", flush=True)

for model, anims in sorted(groups.items()):
    run(os.path.splitext(model)[0], model, anims)
for f in standalone:
    run(os.path.splitext(f)[0], f, [])

json.dump(dict(groups=groups, standalone=standalone, results=report), open(os.path.join(out_dir, 'report.json'), 'w'), indent=1)
bad = [k for k, v in report.items() if not v['ok']]
print(f"\nmodels {len(report)}  failed {len(bad)}  animations {sum(v['animations'] for v in report.values())}  "
      f"failed animations {sum(len(v['failed_animations']) for v in report.values())}")
