#!/usr/bin/env python3
"""Read / compare grntest output (.gtst).

  grntest.py dump a.gtst                 print the records
  grntest.py cmp ref.gtst ours.gtst      compare (result codes exactly, floats with max abs error)
"""
import struct, sys
import numpy as np


def read(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'GTST'
    p = 8
    recs = []

    def u32():
        nonlocal p
        v = struct.unpack_from('<I', b, p)[0]; p += 4; return v

    def f64():
        nonlocal p
        v = struct.unpack_from('<d', b, p)[0]; p += 8; return v

    def raw(n):
        nonlocal p
        v = b[p:p + n]; p += n; return v

    while p < len(b):
        tag = b[p:p + 4].decode(); p += 4
        line, res = u32(), struct.unpack('<i', struct.pack('<I', u32()))[0]
        r = dict(tag=tag, line=line, result=res)
        if tag == 'RNDR':
            n = u32(); r['states'] = []
            for _ in range(n):
                st = dict(result=u32())
                st['hdr'] = [u32() for _ in range(8)]   # lock ok, seq ok, present, opaque, cookie, has_xf, deformed, meshcookie
                st['transform'] = np.frombuffer(raw(64), '<f4')
                st['streams'] = []
                for k in range(6):
                    present, typ, dim, count, unk, nbytes = [u32() for _ in range(6)]
                    data = raw(nbytes)
                    arr = (np.frombuffer(data, '<u2' if typ == 3 else '<f4').reshape(-1, dim) if nbytes else None)
                    st['streams'].append(dict(present=present, type=typ, dim=dim, count=count, unknown=unk, data=arr))
                st['tail'] = [u32() for _ in range(4)]
                r['states'].append(st)
            r['unlock'] = u32()
        elif tag == 'TEXS':
            n = u32(); r['textures'] = []
            for _ in range(n):
                t = dict(result=u32())
                t['hdr'] = [u32() for _ in range(7)]   # version ok, format, w, h, bpp, pitch, size
                t['pixels'] = raw(t['hdr'][6]) if t['result'] == 0 else b''
                t['unlock'] = u32()
                r['textures'].append(t)
        elif tag == 'BONE':
            n = u32(); r['bones'] = []
            for _ in range(n):
                res2, parent = u32(), u32()
                v = np.frombuffer(raw(34 * 4), '<f4')
                r['bones'].append(dict(result=res2, parent=parent, pos=v[0:3], rot=v[3:12], quat=v[12:16],
                                       ss=v[16:25], combined=v[25:34]))
        elif tag == 'GETP':
            r['params'] = [(u32(), f64()) for _ in range(9)]
        elif tag == 'TIME':
            r['time'] = f64()
        elif tag == 'HNDL':
            r['value'] = (u32(), u32(), u32())
        elif tag in ('EXPD', 'BIDX', 'ATCH', 'LEFT', 'TXLF'):
            r['value'] = u32()
        recs.append(r)
        if tag == 'END!':
            break
    return recs


STREAMS = ['positions', 'color0', 'color1', 'texcoords', 'normals', 'triangles']


def compare(ra, rb, verbose=True):
    """returns dict of max errors per stream kind and a list of mismatches"""
    errs = {k: 0.0 for k in STREAMS + ['bones', 'params', 'transform']}
    bad = []
    if len(ra) != len(rb):
        bad.append('record count %d != %d' % (len(ra), len(rb)))
    for a, b in zip(ra, rb):
        where = 'line %d %s' % (a['line'], a['tag'])
        if a['tag'] != b['tag'] or a['result'] != b['result']:
            bad.append('%s: tag/result %s %d vs %s %d' % (where, a['tag'], a['result'], b['tag'], b['result']))
            continue
        if a['tag'] == 'RNDR':
            if len(a['states']) != len(b['states']):
                bad.append('%s: states %d vs %d' % (where, len(a['states']), len(b['states']))); continue
            for i, (sa, sb) in enumerate(zip(a['states'], b['states'])):
                if sa['hdr'] != sb['hdr'] or sa['tail'] != sb['tail'] or sa['result'] != sb['result']:
                    bad.append('%s state %d: header %s %s vs %s %s' % (where, i, sa['hdr'], sa['tail'], sb['hdr'], sb['tail']))
                errs['transform'] = max(errs['transform'], float(np.abs(sa['transform'] - sb['transform']).max()))
                for k, (xa, xb) in enumerate(zip(sa['streams'], sb['streams'])):
                    meta_a = (xa['present'], xa['type'], xa['dim'], xa['count'], xa['unknown'])
                    meta_b = (xb['present'], xb['type'], xb['dim'], xb['count'], xb['unknown'])
                    if meta_a != meta_b:
                        bad.append('%s state %d %s: %s vs %s' % (where, i, STREAMS[k], meta_a, meta_b)); continue
                    if (xa['data'] is None) != (xb['data'] is None):
                        bad.append('%s state %d %s: data presence' % (where, i, STREAMS[k])); continue
                    if xa['data'] is None:
                        continue
                    if xa['data'].shape != xb['data'].shape:
                        bad.append('%s state %d %s: shape %s vs %s' % (where, i, STREAMS[k], xa['data'].shape, xb['data'].shape)); continue
                    if k == 5:
                        if not np.array_equal(xa['data'], xb['data']):
                            bad.append('%s state %d triangles differ' % (where, i))
                    else:
                        e = float(np.abs(xa['data'].astype(np.float64) - xb['data']).max()) if xa['data'].size else 0.0
                        errs[STREAMS[k]] = max(errs[STREAMS[k]], e)
        elif a['tag'] == 'TEXS':
            for i, (ta, tb) in enumerate(zip(a['textures'], b['textures'])):
                if ta['hdr'] != tb['hdr'] or ta['pixels'] != tb['pixels'] or ta['result'] != tb['result']:
                    bad.append('%s texture %d differs %s vs %s' % (where, i, ta['hdr'], tb['hdr']))
            if len(a['textures']) != len(b['textures']):
                bad.append('%s texture count %d vs %d' % (where, len(a['textures']), len(b['textures'])))
        elif a['tag'] == 'BONE':
            for ba, bb in zip(a['bones'], b['bones']):
                if ba['result'] != bb['result'] or ba['parent'] != bb['parent']:
                    bad.append('%s bone result/parent %r vs %r' % (where, (ba['result'], ba['parent']), (bb['result'], bb['parent'])))
                for key in ('pos', 'rot', 'quat', 'ss', 'combined'):
                    errs['bones'] = max(errs['bones'], float(np.abs(ba[key].astype(np.float64) - bb[key]).max()))
        elif a['tag'] == 'GETP':
            for (ra_, va), (rb_, vb) in zip(a['params'], b['params']):
                if ra_ != rb_:
                    bad.append('%s param result %d vs %d' % (where, ra_, rb_))
                errs['params'] = max(errs['params'], abs(va - vb))
        elif 'value' in a and a['value'] != b['value']:
            bad.append('%s value %r vs %r' % (where, a['value'], b['value']))
        elif 'time' in a and a['time'] != b['time']:
            bad.append('%s time %r vs %r' % (where, a['time'], b['time']))
    return errs, bad


if __name__ == '__main__':
    if sys.argv[1] == 'dump':
        for r in read(sys.argv[2]):
            s = '%5d %s %d' % (r['line'], r['tag'], r['result'])
            if r['tag'] == 'RNDR':
                for st in r['states']:
                    s += '\n      hdr %s tail %s' % (st['hdr'], st['tail'])
                    for k, x in enumerate(st['streams']):
                        s += '\n      %-9s present %d type %d dim %d count %d unk %d' % (
                            STREAMS[k], x['present'], x['type'], x['dim'], x['count'], x['unknown'])
                        if x['data'] is not None and x['data'].size:
                            s += '  min %s max %s' % (x['data'].min(0), x['data'].max(0))
            elif r['tag'] == 'TEXS':
                for t in r['textures']:
                    s += '\n      %s %d bytes' % (t['hdr'], len(t['pixels']))
            elif r['tag'] == 'GETP':
                s += ' ' + ' '.join('%.9g' % v for _, v in r['params'])
            elif r['tag'] == 'BONE':
                s += ' %d bones' % len(r['bones'])
            elif 'value' in r:
                s += ' value %r' % (r['value'],)
            elif 'time' in r:
                s += ' time %r' % r['time']
            print(s)
    elif sys.argv[1] == 'cmp':
        errs, bad = compare(read(sys.argv[2]), read(sys.argv[3]))
        for x in bad[:40]:
            print('MISMATCH', x)
        print('mismatches', len(bad))
        print('max abs error:', ' '.join('%s %.3g' % kv for kv in errs.items()))
        sys.exit(1 if bad else (2 if any(v > 0 for v in errs.values()) else 0))
