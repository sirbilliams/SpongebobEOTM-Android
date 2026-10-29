#!/usr/bin/env python3
"""Desktop startup/menu replay. Player files and captures remain outside git."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import zlib


def png(bmp):
    b = bmp.read_bytes()
    offset = struct.unpack_from('<I', b, 10)[0]
    w, h = struct.unpack_from('<ii', b, 18)
    assert struct.unpack_from('<H', b, 28)[0] == 24
    stride = (w * 3 + 3) & ~3
    rows = []
    for y in range(h - 1, -1, -1):
        row = b[offset + y * stride:offset + y * stride + w * 3]
        rgb = bytearray(len(row))
        rgb[0::3], rgb[1::3], rgb[2::3] = row[2::3], row[1::3], row[0::3]
        rows.append(b'\0' + rgb)
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    bmp.with_suffix('.png').write_bytes(b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--build', default='build-asan')
    p.add_argument('--offscreen', action='store_true', help='SDL offscreen fallback if :83 cannot be started')
    p.add_argument('--full-logos', action='store_true')
    args = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    shots = root / '.shots'
    shots.mkdir(exist_ok=True)
    env = os.environ.copy()
    env.pop('WAYLAND_DISPLAY', None)
    env.update(DISPLAY=':83', SDL_AUDIODRIVER='dummy', OMT_DATA=str(root / '.game'),
               OMT_WINDOW_SIZE='640x480', OMT_CONFIG=str(shots / 'settings.cfg'))
    if args.offscreen:
        env['SDL_VIDEODRIVER'] = 'offscreen'
    else:
        env.pop('SDL_VIDEODRIVER', None)
    # LSan cannot attach threads in the Codex sandbox; ASan and UBSan remain enabled.
    env.setdefault('ASAN_OPTIONS', 'detect_leaks=0:halt_on_error=1')
    env.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
    env['OMT_AUTOKEYS'] = '2:+Space,4:+Space,6:+Space,8:+Space,10:+Space'
    env['OMT_FRAMES'] = '180'
    env['OMT_SHOTS'] = '12,16,20,24,32,40,48,56,64,72,80,88,96'
    env['OMT_SCREENSHOT'] = str(shots / 'menu-replay.bmp')
    env['OMT_MENU_CLICKS'] = ','.join([
        '4:320:100', '8:470:375', '12:320:388',
        '16:320:140', '20:240:128', '24:270:298', '28:320:388',
        '32:320:180', '36:365:388', '40:320:220', '44:365:388',
        '48:320:260', '52:320:388', '56:320:300', '60:365:260',
        '64:320:340', '68:320:380', '72:270:260'])
    binary = root / args.build / 'spongebob'
    def run(name):
        with (shots / (name + '.log')).open('w') as log:
            subprocess.run([str(binary)], env=env, cwd=root, stdout=log, stderr=log, check=True, timeout=150)
        text = (shots / (name + '.log')).read_text()
        assert 'runtime error:' not in text and 'ERROR: AddressSanitizer' not in text
        assert 'main menu: clean exit' in text
        return text
    text = run('menu-replay')
    for i in range(8):
        assert 'click menu item %d ->' % i in text, 'missing item %d' % i
    assert 'confirmation Yes (7)' in text
    assert 'volume 0=1' in text
    # New game confirmation -> intro -> parsed first scene -> explicit VM/3D boundary.
    env.update(OMT_AUTOKEYS=env['OMT_AUTOKEYS'] + ',30:+Space', OMT_SHOTS='12,18,24,35',
               OMT_MENU_CLICKS='4:320:300,8:270:260,32:320:380,36:270:260',
               OMT_SCREENSHOT=str(shots / 'new-game.bmp'))
    text = run('new-game')
    assert 'Switch to scene mintro' in text and 'c1l0s1 parsed' in text
    if args.full_logos:
        env.pop('OMT_AUTOKEYS', None)
        env.update(OMT_FRAMES='950', OMT_SHOTS='20,125,225,350,450,915,925',
                   OMT_MENU_CLICKS='8:320:380,12:270:260',
                   OMT_SCREENSHOT=str(shots / 'logos-menu.bmp'))
        text = run('logos-menu')
        for movie in ('mlegal', 'mthq', 'mawe', 'mnick', 'mspongebob'):
            assert 'movie %s finished\n' % movie in text
    for bmp in shots.glob('*.bmp'):
        png(bmp)
    print('PASS: eight actions, hover, options, confirmations, intro/scene boundary, clean exit' +
          (', full logos' if args.full_logos else ''))

if __name__ == '__main__':
    main()
