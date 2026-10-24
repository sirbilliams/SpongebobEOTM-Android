"""Reader for RAD Granny 1.x (.grn, "1.2b", 2000) files as used by Jimmy Neutron vs. Jimmy Negatron.

Layout (little-endian), as far as the game's files use it:

  0x00  32-byte magic, 0x20 more bytes of header
  0x40  u32 0xca5e0000, u32 nsections, u32 crc, u32 0, u32 size, ...
  0x60  nsections x { u32 tag, u32 0, u32 offset, u32 crc, u32 0 }
  section: u32 nentries, u32 0, u32 ?, u32 0, then nentries x { u32 tag, u32 offset, u32 ndesc }
           (offset relative to the section start, ndesc = number of descendants: a pre-order tree).
           A node's own data runs from its offset to the offset of the next entry.

Tags (low 16 bits of 0xca5eXXXX) that matter:
  0200 string table (u32 count, u32 bytes, NUL-terminated strings)
  0f03 object properties: 0f00 object { u32 ?, u32 ? } > 0f05 > 0f01 {u32 key} > 0f06 > 0f02 {u32 type(0 = string), u32 value}
       objects are numbered 1.. in file order; 0f04 { u32 object } attaches an object (its __ObjectName) to the parent
  0b01 tool nodes: 0b00 > 0f04 (node i -> object)
  0507 > 0505 skeleton > 0508 > 0506 bone { u32 parent, f32 pos[3], f32 quat[4] (x,y,z,w), f32 scale_shear[9] }
  0602 > 0601 mesh > 0604 > 0603 > { 0801 f32 pos[n][3], 0802 f32 normal[m][3], 0804 > 0803 { u32 dim, f32 uv[k][dim] } }
                     0702 skin { u32 nverts, u32 max_binding, u32 max_influences, per vertex { u32 n, n x { u32 binding, f32 w } } }
                     0901 triangles { per triangle u32 pos[3], u32 normal[3] }, 0f04
  0304 > 0301 texture { u32 w, u32 h, u32 format, ... Bink-compressed image }, 0f04
  0d01 > 0d00 material { u32 ?, u32 ?, u32 ? }, 0f04
  0c01 > 0c00 model > 0c07 > 0c08 { u32 skeleton (1-based) }, 0c02 { u32 tool_node (1-based) per bone }, 0c05
                    > 0c06 > 0c03 { u32 mesh (1-based) } > 0c04, 0c09 > 0c0a binding { u32 bone, f32 obb[7] }
  0e01 > 0e00 > 0e07 > 0e02 { u32 ?, u32 ? } > 0e03 > { 0e05 f32 rgb[3], 0e04 u32 }, 0e06 { u32 n, n x { u32 tri, u32 uv[3] } }
  1205 > 1200 animation > 1201, 1203 > 1204 track { u32 tool_node, u32 ?, u32 ?, u32 degree[3], u32 nknots[3],
                     u32 knot_array[3], u32 ?, f32 knots..., pos cps (3 f32), quat cps (4 f32), scale_shear cps (9 f32) }
"""
import struct


class Node:
    __slots__ = ('tag', 'off', 'end', 'kids', 'parent')

    def __init__(self, tag, off):
        self.tag, self.off, self.end, self.kids, self.parent = tag, off, off, [], None

    def find(self, tag):
        return [k for k in self.kids if k.tag == tag]

    def first(self, tag):
        r = self.find(tag)
        return r[0] if r else None

    def walk(self):
        yield self
        for k in self.kids:
            yield from k.walk()


class GrnFile:
    def __init__(self, path):
        self.path = path
        self.b = b = open(path, 'rb').read()
        if struct.unpack_from('<I', b, 0x40)[0] != 0xca5e0000:
            raise ValueError('not a Granny 1.x file: %s' % path)
        nsec = self.u32(0x44)
        self.sections = []
        for i in range(nsec):
            tag, _, off, _, _ = struct.unpack_from('<5I', b, 0x60 + i * 0x14)
            n = self.u32(off)
            ents = [struct.unpack_from('<3I', b, off + 0x10 + k * 12) for k in range(n)]
            nodes = [Node(t & 0xffff, off + o) for t, o, c in ents]
            for k in range(n):
                nodes[k].end = nodes[k + 1].off if k + 1 < n else len(b)
            roots, stack = [], []
            for k, (t, o, c) in enumerate(ents):
                while stack and stack[-1][1] < k:
                    stack.pop()
                if stack:
                    nodes[k].parent = stack[-1][0]
                    stack[-1][0].kids.append(nodes[k])
                else:
                    roots.append(nodes[k])
                stack.append((nodes[k], k + c))
            self.sections.append((tag & 0xffff, off, roots))
        main = [r for t, o, r in self.sections if t == 0x0103]
        self.roots = main[0] if main else []
        st = [r for r in self.roots if r.tag == 0x0200]
        self.strings = self._strtab(st[0].off) if st else []
        self._objects()

    # -- raw helpers
    def u32(self, o):
        return struct.unpack_from('<I', self.b, o)[0]

    def u32s(self, o, n):
        return list(struct.unpack_from('<%dI' % n, self.b, o))

    def f32s(self, o, n):
        return list(struct.unpack_from('<%df' % n, self.b, o))

    def _strtab(self, off):
        n = self.u32(off)
        p, s = off + 8, []
        for _ in range(n):
            e = self.b.index(b'\0', p)
            s.append(self.b[p:e].decode('latin1'))
            p = e + 1
        return s

    def top(self, tag):
        return [r for r in self.roots if r.tag == tag]

    def _objects(self):
        self.objects = [None]       # 1-based
        for r in self.top(0x0f03):
            for obj in r.find(0x0f00):
                props = {}
                for lst in obj.find(0x0f05):
                    for p in lst.find(0x0f01):
                        key = self.strings[self.u32(p.off)]
                        v = [n for n in p.walk() if n.tag == 0x0f02]
                        if v:
                            typ, val = self.u32s(v[0].off, 2)
                            props[key] = self.strings[val] if typ == 0 else val
                self.objects.append(props)

    def obj_name(self, node):
        """__ObjectName of the object attached to node by a 0f04 child (or the node itself if it is 0f04)."""
        ref = node if node.tag == 0x0f04 else node.first(0x0f04)
        if ref is None:
            return None
        i = self.u32(ref.off)
        return self.objects[i].get('__ObjectName') if 0 < i < len(self.objects) else None

    # -- semantic accessors
    def tool_nodes(self):
        """names of tool nodes (0b00), 0-based"""
        out = []
        for r in self.top(0x0b01):
            for n in r.find(0x0b00):
                out.append(self.obj_name(n))
        return out

    def skeletons(self):
        out = []
        for r in self.top(0x0507):
            for sk in r.find(0x0505):
                bones = []
                for lst in sk.find(0x0508):
                    for bn in lst.find(0x0506):
                        v = self.b[bn.off:bn.off + 0x44]
                        parent = struct.unpack_from('<I', v, 0)[0]
                        f = struct.unpack_from('<16f', v, 4)
                        bones.append(dict(parent=parent, pos=list(f[0:3]), quat=list(f[3:7]), ss=list(f[7:16]),
                                          raw=v))
                out.append(bones)
        return out

    def meshes(self):
        out = []
        for r in self.top(0x0602):
            for m in r.find(0x0601):
                d = dict(name=self.obj_name(m))
                vd = [n for n in m.walk() if n.tag == 0x0603]
                v = vd[0]
                pn, nn, un = v.first(0x0801), v.first(0x0802), v.first(0x0804)
                d['pos'] = self.f32s(pn.off, 3 * ((pn.end - pn.off) // 12)) if pn else []
                d['nrm'] = self.f32s(nn.off, 3 * ((nn.end - nn.off) // 12)) if nn else []
                d['uv_layers'] = []
                for ul in v.find(0x0804):
                    for u in ul.find(0x0803):
                        dim = self.u32(u.off)
                        cnt = (u.end - u.off - 4) // (4 * dim) if dim else 0
                        d['uv_layers'].append(dict(dim=dim, uv=self.f32s(u.off + 4, dim * cnt)))
                sk = m.first(0x0702)
                d['skin'] = None
                if sk:
                    nv, mb, mi = self.u32s(sk.off, 3)
                    p, w = sk.off + 12, []
                    for _ in range(nv):
                        n = self.u32(p); p += 4
                        inf = []
                        for _ in range(n):
                            bi, = struct.unpack_from('<I', self.b, p)
                            wt, = struct.unpack_from('<f', self.b, p + 4)
                            inf.append((bi, wt)); p += 8
                        w.append(inf)
                    d['skin'] = dict(nverts=nv, max_binding=mb, max_influences=mi, weights=w)
                tr = m.first(0x0901)
                ntri = (tr.end - tr.off) // 24 if tr else 0
                d['tris'] = [self.u32s(tr.off + 24 * i, 6) for i in range(ntri)]
                out.append(d)
        return out

    def textures(self):
        out = []
        for r in self.top(0x0304):
            for t in r.find(0x0301):
                w, h, fmt = self.u32s(t.off, 3)
                out.append(dict(name=self.obj_name(t), w=w, h=h, format=fmt))
        return out

    def materials(self):
        out = []
        for r in self.top(0x0d01):
            for m in r.find(0x0d00):
                d = [n for n in m.walk() if n.tag == 0x0d03]
                out.append(dict(name=self.obj_name(m), raw=self.u32s(d[0].off, 3) if d else None))
        return out

    def texture_maps(self):
        out = []
        for r in self.top(0x0e01):
            for m in r.find(0x0e00):
                d = dict()
                e2 = [n for n in m.walk() if n.tag == 0x0e02]
                d['e02'] = self.u32s(e2[0].off, 2) if e2 else None
                e5 = [n for n in m.walk() if n.tag == 0x0e05]
                d['color'] = self.f32s(e5[0].off, 3) if e5 else None
                e4 = [n for n in m.walk() if n.tag == 0x0e04]
                d['e04'] = self.u32(e4[0].off) if e4 else None
                e6 = [n for n in m.walk() if n.tag == 0x0e06]
                if e6:
                    n = self.u32(e6[0].off)
                    d['uv_tris'] = [self.u32s(e6[0].off + 4 + 16 * i, 4) for i in range(n)]
                else:
                    d['uv_tris'] = []
                out.append(d)
        return out

    def models(self):
        out = []
        for r in self.top(0x0c01):
            for m in r.find(0x0c00):
                d = dict(skeleton=None, bone_nodes=[], meshes=[])
                for a in m.find(0x0c07):
                    s8 = [n for n in a.walk() if n.tag == 0x0c08]
                    d['skeleton'] = self.u32(s8[0].off) if s8 and s8[0].end > s8[0].off else None
                    c2 = [n for n in a.walk() if n.tag == 0x0c02]
                    if c2:
                        d['bone_nodes'] = self.u32s(c2[0].off, (c2[0].end - c2[0].off) // 4)
                for a in m.find(0x0c06):
                    for c3 in a.find(0x0c03):
                        md = dict(mesh=self.u32(c3.off), bindings=[])
                        for c9 in [n for n in c3.walk() if n.tag == 0x0c09]:
                            for ca in c9.find(0x0c0a):
                                md['bindings'].append(dict(bone=self.u32(ca.off), obb=self.f32s(ca.off + 4, 7)))
                        d['meshes'].append(md)
                out.append(d)
        return out

    def animations(self):
        out = []
        for r in self.top(0x1205):
            for a in r.find(0x1200):
                tracks = []
                for lst in a.find(0x1203):
                    for t in lst.find(0x1204):
                        tracks.append(self._track(t))
                out.append(dict(tracks=tracks))
        return out

    def _track(self, t):
        node, a, kind = self.u32s(t.off, 3)
        if kind == 0:
            # raw motion: keys { f32 time, pos[3], quat[4], scale_shear[9] }
            n = (t.end - t.off - 12) // 68
            keys = [self.f32s(t.off + 12 + 68 * i, 17) for i in range(n)]
            return dict(node=node, a=a, b=kind, kind='raw', keys=keys)
        h = self.u32s(t.off, 13)
        node, a, b = h[0:3]
        deg, nk, ki, extra = h[3:6], h[6:9], h[9:12], h[12]
        p = t.off + 13 * 4
        knots = []
        for n in nk:
            knots.append(self.f32s(p, n)); p += 4 * n
        curves = []
        for c, dim in enumerate((3, 4, 9)):
            n = nk[ki[c]]
            curves.append(dict(degree=deg[c], knots=ki[c], cps=self.f32s(p, dim * n)))
            p += 4 * dim * n
        assert p == t.end or t.end - p < 4, (hex(p), hex(t.end))
        return dict(node=node, a=a, b=b, kind='bspline', extra=extra, knot_arrays=knots, curves=curves)


if __name__ == '__main__':
    import sys
    g = GrnFile(sys.argv[1])
    tn = g.tool_nodes()
    print('tool nodes', len(tn))
    for i, sk in enumerate(g.skeletons()):
        print('skeleton', i + 1, 'bones', len(sk))
    for mo in g.models():
        sk = g.skeletons()[mo['skeleton'] - 1] if mo['skeleton'] else []
        names = [tn[k - 1] if 0 < k <= len(tn) else None for k in mo['bone_nodes']]
        print('model: skeleton', mo['skeleton'], 'bones', len(mo['bone_nodes']))
        for bi, b in enumerate(sk):
            print('   %2d %-22s parent %2d pos %s' % (bi, names[bi] if bi < len(names) else '?', b['parent'],
                                                    ' '.join('%.4g' % x for x in b['pos'])))
        for md in mo['meshes']:
            print('   mesh', md['mesh'], 'bindings', [(x['bone'], names[x['bone']] if x['bone'] < len(names) else '?') for x in md['bindings']])
    for m in g.meshes():
        print('mesh', m['name'], 'pos', len(m['pos']) // 3, 'nrm', len(m['nrm']) // 3,
              'uv', [(u['dim'], len(u['uv']) // max(1, u['dim'])) for u in m['uv_layers']], 'tris', len(m['tris']),
              'skin', m['skin'] and (m['skin']['nverts'], m['skin']['max_binding'], m['skin']['max_influences']))
    print('textures', g.textures())
    print('materials', g.materials())
    for tm in g.texture_maps():
        print('texmap', {k: v for k, v in tm.items() if k != 'uv_tris'}, 'uv tris', len(tm['uv_tris']))
    for a in g.animations():
        print('animation tracks', len(a['tracks']))
        for t in a['tracks'][:50]:
            if t['kind'] == 'raw':
                print('   node %2d %-22s raw keys %d  t %s..%s' % (t['node'], tn[t['node'] - 1], len(t['keys']), t['keys'][0][0], t['keys'][-1][0]))
                continue
            print('   node %2d %-22s a=%d b=%d extra=%d deg %s nknots %s dur %s' % (
                t['node'], tn[t['node'] - 1] if 0 < t['node'] <= len(tn) else '?', t['a'], t['b'], t['extra'],
                [c['degree'] for c in t['curves']], [len(k) for k in t['knot_arrays']],
                [k[-1] if k else None for k in t['knot_arrays']]))
