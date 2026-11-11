#!/usr/bin/env bash
# Desktop equivalent of engine/importer/SbImport.c. Requires python3 and unshield.
set -euo pipefail
[[ $# == 2 ]] || { echo "usage: $0 <iso> <dest>" >&2; exit 2; }
command -v unshield >/dev/null
python3 - "$1" "$2" <<'PY'
import pathlib, shutil, struct, subprocess, sys, tempfile

iso, dest = sys.argv[1], pathlib.Path(sys.argv[2]).resolve()
if dest.exists() and any(dest.iterdir()):
    sys.exit('Destination must be empty (existing saves are never overwritten).')
dest.mkdir(parents=True, exist_ok=True)

with open(iso, 'rb') as disc, tempfile.TemporaryDirectory(prefix='sb-install-') as tmp:
    tmp = pathlib.Path(tmp)
    def read(offset, size):
        disc.seek(offset)
        data = disc.read(size)
        if len(data) != size:
            raise ValueError('Truncated ISO')
        return data
    def extent(record):
        return struct.unpack_from('<I', record, 2)[0] * 2048, struct.unpack_from('<I', record, 10)[0]
    root = None
    joliet = False
    for sector in range(16, 32):
        vd = read(sector * 2048, 2048)
        if vd[1:6] != b'CD001' or vd[0] == 255:
            break
        is_joliet = vd[0] == 2 and vd[88:91] in (b'%/@', b'%/C', b'%/E')
        if (vd[0] == 1 and root is None) or is_joliet:
            root, joliet = extent(vd[156:190]), is_joliet
    if root is None:
        sys.exit('Not an ISO 9660 disc image')
    def entries(where):
        data = read(*where)
        pos = 0
        while pos < len(data):
            length = data[pos]
            if not length:
                pos = (pos // 2048 + 1) * 2048
                continue
            record = data[pos:pos + length]
            pos += length
            name = record[33:33 + record[32]]
            if name in (b'\0', b'\1'):
                continue
            name = name.decode('utf-16-be' if joliet else 'ascii').split(';')[0]
            if '/' in name or '\\' in name or name in ('.', '..'):
                raise ValueError('Unsafe ISO path')
            yield name, bool(record[25] & 2), extent(record)
    top = {n.lower(): (d, e) for n, d, e in entries(root)}
    def save(where, path):
        offset, remaining = where
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('wb') as out:
            while remaining:
                size = min(remaining, 1024 * 1024)
                out.write(read(offset, size))
                offset += size
                remaining -= size
    for name in ('data1.hdr', 'data1.cab'):
        save(top[name][1], tmp / name)
    for name, directory, where in entries(top['movies'][1]):
        if not directory and name.lower().endswith('.bik'):
            save(where, dest / 'movies' / name)
    helpfile = next(e for n, d, e in entries(top['sounds'][1]) if not d and n.lower() == 'help.omt')
    save(helpfile, dest / 'sounds/help.omt')
    subprocess.run(['unshield', '-d', str(tmp / 'unpacked'), '-g', 'Program Executable Files',
                    'x', str(tmp / 'data1.cab')], check=True, stdout=subprocess.DEVNULL)
    group = tmp / 'unpacked/Program_Executable_Files'
    if not (group / 'sb2.exe').is_file():
        raise ValueError('Installer group does not contain sb2.exe')
    shutil.copytree(group, dest, dirs_exist_ok=True)
    (dest / 'cd/sounds').mkdir(parents=True, exist_ok=True)
    shutil.copy2(dest / 'sounds/help.omt', dest / 'cd/sounds/help.omt')
    if not (dest / 'scenes').is_dir() or not list((dest / 'music').glob('*.omt')):
        raise ValueError('Incomplete game layout')
print(f'Installed game, movies and hint audio into {dest}')
PY
