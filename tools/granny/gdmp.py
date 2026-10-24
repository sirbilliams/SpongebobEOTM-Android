"""Read grndump output (.gdmp) and render previews / export OBJ.

  python3 gdmp.py info  x.gdmp
  python3 gdmp.py render x.gdmp out.png [pose] [frame]      (textured software render)
  python3 gdmp.py sheet  x.gdmp out.png                     (bind pose + frames of every pose)
  python3 gdmp.py obj    x.gdmp outdir                      (bind pose -> OBJ + PNG textures)
"""
import struct, sys, os, math
import numpy as np
from PIL import Image


def read(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'GDMP', 'not a gdmp'
    p = 8
    def u32():
        nonlocal p; v = struct.unpack_from('<I', b, p)[0]; p += 4; return v
    def i32():
        nonlocal p; v = struct.unpack_from('<i', b, p)[0]; p += 4; return v
    def f32():
        nonlocal p; v = struct.unpack_from('<f', b, p)[0]; p += 4; return v
    textures = []
    for _ in range(u32()):
        w, h = u32(), u32()
        textures.append(Image.frombytes('RGB', (w, h), b[p:p + w * h * 3]) if w and h else None)
        p += w * h * 3
    poses = []
    for _ in range(u32()):
        n = u32(); name = b[p:p + n].decode('latin1'); p += n
        nframes = u32(); secs = f32()
        frames = []
        for _ in range(nframes):
            states = []
            for _ in range(u32()):
                tex = i32(); nv = u32()
                v = np.frombuffer(b, '<f4', nv * 9, p).reshape(nv, 9); p += nv * 36
                nt = u32()
                t = np.frombuffer(b, '<u2', nt * 3, p).reshape(nt, 3); p += nt * 6
                states.append(dict(tex=tex, pos=v[:, 0:3], nrm=v[:, 3:6], uv=v[:, 6:8], tri=t))
            frames.append(states)
        poses.append(dict(name=name, secs=secs, frames=frames))
    return textures, poses


def render(textures, states, size=320, yaw=30, pitch=10, bounds=None):
    """Tiny z-buffered textured rasteriser (orthographic, Y up)."""
    allp = np.concatenate([s['pos'] for s in states]) if states else np.zeros((1, 3))
    if bounds is None:
        bounds = (allp.min(0), allp.max(0))
    mn, mx = bounds
    c = (mn + mx) / 2; ext = max((mx - mn).max(), 1e-6)
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    R = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]) @ np.eye(3)
    R = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]]) @ R
    img = np.full((size, size, 3), 40, np.uint8); zb = np.full((size, size), -1e30)
    light = np.array([0.4, 0.7, 0.6]); light /= np.linalg.norm(light)
    for s in states:
        P = (s['pos'] - c) @ R.T
        sx = (P[:, 0] / ext * 0.9 + 0.5) * size; syy = (0.5 - P[:, 1] / ext * 0.9) * size; z = P[:, 2]
        tex = textures[s['tex']] if 0 <= s['tex'] < len(textures) and textures[s['tex']] else None
        ta = np.asarray(tex) if tex else None
        N = s['nrm'] @ R.T
        for a, b2, d in s['tri']:
            xs = np.array([sx[a], sx[b2], sx[d]]); ys = np.array([syy[a], syy[b2], syy[d]])
            x0, x1 = int(max(0, xs.min())), int(min(size - 1, xs.max()))
            y0, y1 = int(max(0, ys.min())), int(min(size - 1, ys.max()))
            if x1 < x0 or y1 < y0: continue
            den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(den) < 1e-9: continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            l0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den
            l1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den
            l2 = 1 - l0 - l1
            m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not m.any(): continue
            zz = l0 * z[a] + l1 * z[b2] + l2 * z[d]
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            m &= zz > sub
            if not m.any(): continue
            n = N[a] + N[b2] + N[d]; nl = np.linalg.norm(n)
            shade = 0.45 + 0.55 * abs(float(n @ light) / nl) if nl > 0 else 1.0
            if ta is not None:
                u = l0 * s['uv'][a, 0] + l1 * s['uv'][b2, 0] + l2 * s['uv'][d, 0]
                v = l0 * s['uv'][a, 1] + l1 * s['uv'][b2, 1] + l2 * s['uv'][d, 1]
                tx = (np.mod(u, 1.0) * (ta.shape[1] - 1)).astype(int)
                ty = (np.mod(v, 1.0) * (ta.shape[0] - 1)).astype(int)
                col = ta[ty, tx].astype(float)
            else:
                col = np.full(gx.shape + (3,), 190.0)
            region = img[y0:y1 + 1, x0:x1 + 1]
            region[m] = np.clip(col[m] * shade, 0, 255).astype(np.uint8)
            sub[m] = zz[m]
    return Image.fromarray(img)


def pose_bounds(poses):
    allp = np.concatenate([s['pos'] for pz in poses for f in pz['frames'] for s in f])
    return allp.min(0), allp.max(0)


if __name__ == '__main__':
    cmd, path = sys.argv[1], sys.argv[2]
    textures, poses = read(path)
    if cmd == 'info':
        print('textures', [t.size if t else None for t in textures])
        for pz in poses:
            f0 = pz['frames'][0] if pz['frames'] else []
            print(f"pose {pz['name']:<28} frames {len(pz['frames']):4d} {pz['secs']:.2f}s  states {len(f0)}  "
                  f"verts {sum(len(s['pos']) for s in f0)}  tris {sum(len(s['tri']) for s in f0)}  tex {[s['tex'] for s in f0]}")
    elif cmd == 'render':
        pi = int(sys.argv[4]) if len(sys.argv) > 4 else 0; fi = int(sys.argv[5]) if len(sys.argv) > 5 else 0
        render(textures, poses[pi]['frames'][fi], 512).save(sys.argv[3])
    elif cmd == 'sheet':
        bnd = pose_bounds(poses); cells = []
        for pz in poses:
            n = len(pz['frames'])
            for k in ([0] if n == 1 else [0, n // 4, n // 2, 3 * n // 4]):
                cells.append(render(textures, pz['frames'][k], 200, bounds=bnd))
        W = 4; H = (len(cells) + W - 1) // W
        sheet = Image.new('RGB', (W * 200, H * 200), (20, 20, 20))
        for i, c in enumerate(cells): sheet.paste(c, ((i % W) * 200, (i // W) * 200))
        sheet.save(sys.argv[3])
    elif cmd == 'obj':
        od = sys.argv[3]; os.makedirs(od, exist_ok=True)
        base = os.path.splitext(os.path.basename(path))[0]
        for i, t in enumerate(textures):
            if t: t.save(os.path.join(od, f'{base}_tex{i}.png'))
        with open(os.path.join(od, base + '.mtl'), 'w') as m:
            for i in range(len(textures)): m.write(f'newmtl tex{i}\nmap_Kd {base}_tex{i}.png\n')
        with open(os.path.join(od, base + '.obj'), 'w') as o:
            o.write(f'mtllib {base}.mtl\n'); off = 1
            for s in poses[0]['frames'][0]:
                for v in s['pos']: o.write('v %f %f %f\n' % tuple(v))
                for v in s['uv']: o.write('vt %f %f\n' % (v[0], 1 - v[1]))
                for v in s['nrm']: o.write('vn %f %f %f\n' % tuple(v))
                o.write(f"usemtl tex{s['tex']}\n")
                for t in s['tri']:
                    a, b2, c = t + off; o.write(f'f {a}/{a}/{a} {b2}/{b2}/{b2} {c}/{c}/{c}\n')
                off += len(s['pos'])
