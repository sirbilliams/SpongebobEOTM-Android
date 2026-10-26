#!/usr/bin/env python3
"""Launcher icon for the port, drawn for this project (no show art): an "employee of the month" award
ribbon with a fry cook's spatula, on an underwater background with light rays and bubbles.

One shape list -> Android adaptive-icon vector drawables (background, foreground, monochrome) and an SVG
preview of the composed icon.

  python3 tools/icon/make_icon.py android/app/src/main/res preview.svg
"""
import math, os, sys

res, preview = sys.argv[1], sys.argv[2]
C = 54.0                                   # 108x108 viewport, adaptive-icon safe zone = circle r=33

def circle(cx, cy, r):
    return f'M{cx - r:.2f},{cy:.2f} a{r:.2f},{r:.2f} 0 1,0 {2 * r:.2f},0 a{r:.2f},{r:.2f} 0 1,0 {-2 * r:.2f},0 Z'

def rrect(x, y, w, h, r):
    return (f'M{x + r:.2f},{y:.2f} H{x + w - r:.2f} A{r:.2f},{r:.2f} 0 0,1 {x + w:.2f},{y + r:.2f} '
            f'V{y + h - r:.2f} A{r:.2f},{r:.2f} 0 0,1 {x + w - r:.2f},{y + h:.2f} H{x + r:.2f} '
            f'A{r:.2f},{r:.2f} 0 0,1 {x:.2f},{y + h - r:.2f} V{y + r:.2f} A{r:.2f},{r:.2f} 0 0,1 {x + r:.2f},{y:.2f} Z')

def scallop(cx, cy, r, bumps, depth):
    pts = []
    for i in range(bumps * 8):
        a = 2 * math.pi * i / (bumps * 8) - math.pi / 2
        rr = r + depth * (0.5 + 0.5 * math.cos(bumps * a))
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return 'M' + ' L'.join(f'{x:.2f},{y:.2f}' for x, y in pts) + ' Z'

def lin(x0, y0, x1, y1, *stops):
    return dict(kind='lin', x0=x0, y0=y0, x1=x1, y1=y1, stops=stops)

def rad(cx, cy, r, *stops):
    return dict(kind='rad', cx=cx, cy=cy, r=r, stops=stops)

# ---------------------------------------------------------------- background
bg = [dict(d='M0,0 H108 V108 H0 Z', fill=lin(0, 0, 0, 108, (0, '#FF35C2EE'), (0.55, '#FF1784C4'), (1, '#FF0A4C86')))]
for x0, w in ((14, 9), (40, 6), (66, 10), (90, 7)):         # light rays from the surface
    bg.append(dict(d=f'M{x0},0 h{w} l{w * 1.6:.1f},108 h{-w * 2.4:.1f} Z',
                   fill=lin(0, 0, 0, 108, (0, '#38FFFFFF'), (1, '#00FFFFFF'))))
bg.append(dict(d='M0,94 C18,88 34,99 54,94 S88,88 108,95 V108 H0 Z', fill='#FFE8C77E'))   # sand
bg.append(dict(d='M0,99 C20,95 38,103 58,99 S92,94 108,100 V108 H0 Z', fill='#FFD9AF5F'))
for cx, cy, r in ((14, 30, 3.2), (19, 21, 2.0), (13, 13, 1.3), (93, 70, 3.0), (89, 60, 1.9), (96, 52, 1.2),
                  (85, 16, 2.2), (22, 78, 1.6)):
    bg.append(dict(d=circle(cx, cy, r), fill='#30FFFFFF', stroke='#B0FFFFFF', sw=0.7))
    bg.append(dict(d=circle(cx - r * 0.35, cy - r * 0.35, r * 0.28), fill='#C0FFFFFF'))

# ---------------------------------------------------------------- foreground: award ribbon + spatula
MY = 49.0                                   # medal centre
fg = [
    # ribbon tails
    dict(d=f'M{C - 13},{MY + 10} L{C - 19},{MY + 37} L{C - 12},{MY + 32} L{C - 6},{MY + 37} L{C - 1},{MY + 14} Z',
         fill=lin(0, MY + 10, 0, MY + 37, (0, '#FFE5383B'), (1, '#FFB0202A'))),
    dict(d=f'M{C + 13},{MY + 10} L{C + 19},{MY + 37} L{C + 12},{MY + 32} L{C + 6},{MY + 37} L{C + 1},{MY + 14} Z',
         fill=lin(0, MY + 10, 0, MY + 37, (0, '#FFE5383B'), (1, '#FFB0202A'))),
    # drop shadow, scalloped rosette, gold medal
    dict(d=scallop(C + 0.8, MY + 1.6, 23.0, 14, 2.6), fill='#40001A33'),
    dict(d=scallop(C, MY, 23.0, 14, 2.6), fill=lin(30, 25, 78, 75, (0, '#FFFFE07A'), (0.5, '#FFF2AE2B'), (1, '#FFC9820F'))),
    dict(d=circle(C, MY, 18.5), fill=rad(C - 5, MY - 6, 24, (0, '#FFFFF3B8'), (0.6, '#FFFFD25A'), (1, '#FFE9A91F')),
         stroke='#FFB8770C', sw=1.2),
    dict(d=circle(C, MY, 15.6), stroke='#80FFFFFF', sw=0.6),
]
# spatula, drawn upright and rotated about the medal centre
spat = [
    dict(d=rrect(C - 1.9, MY - 1, 3.8, 17.5, 1.9), fill=lin(C - 1.9, 0, C + 1.9, 0, (0, '#FF9A6334'), (1, '#FF6E4222'))),  # handle
    dict(d=rrect(C - 1.2, MY - 4.5, 2.4, 5, 0.6), fill='#FF9AA6B0'),                                                       # neck
    dict(d=rrect(C - 7, MY - 18, 14, 14.5, 2.6), fill=lin(C - 7, 0, C + 7, 0, (0, '#FFF6FAFD'), (1, '#FFB9C6D2')),
         stroke='#FF6F7E8C', sw=0.8),                                                                                    # blade
]
for sx in (-3.6, 0, 3.6):                                                                                              # slots
    spat.append(dict(d=rrect(C + sx - 0.7, MY - 15.5, 1.4, 9, 0.7), fill='#FF6F7E8C'))
spat.append(dict(d=circle(C, MY + 14, 0.9), fill='#FF3D2410'))                                                        # hang hole
fg.append(dict(group=spat, rotate=-35))
# a small star above the spatula blade
def star(cx, cy, ro, ri):
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = ro if i % 2 == 0 else ri
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return 'M' + ' L'.join(f'{x:.2f},{y:.2f}' for x, y in pts) + ' Z'
fg.append(dict(d=star(C + 9.5, MY - 8.5, 3.6, 1.5), fill='#FFFFFFFF'))

mono = [dict(d=scallop(C, MY, 23.0, 14, 2.6) + ' ' + circle(C, MY, 18.0), fill='#FFFFFFFF', evenodd=True),
        dict(d=f'M{C - 13},{MY + 12} L{C - 19},{MY + 37} L{C - 12},{MY + 32} L{C - 6},{MY + 37} L{C - 2},{MY + 19} Z '
               f'M{C + 13},{MY + 12} L{C + 19},{MY + 37} L{C + 12},{MY + 32} L{C + 6},{MY + 37} L{C + 2},{MY + 19} Z',
             fill='#FFFFFFFF'),
        dict(group=[dict(d=rrect(C - 1.9, MY - 1, 3.8, 15, 1.9), fill='#FFFFFFFF'),
                    dict(d=rrect(C - 1.2, MY - 4.5, 2.4, 5, 0.6), fill='#FFFFFFFF'),
                    dict(d=rrect(C - 6.5, MY - 17, 13, 13, 2.4), fill='#FFFFFFFF')], rotate=-35)]

# ---------------------------------------------------------------- writers
def argb_to_svg(c):
    a = int(c[1:3], 16) / 255.0
    return '#' + c[3:], a

def vd_fill(attr, f):
    if isinstance(f, str):
        return f' android:{attr}="{f}"', ''
    if f['kind'] == 'lin':
        g = (f'<gradient android:type="linear" android:startX="{f["x0"]}" android:startY="{f["y0"]}" '
             f'android:endX="{f["x1"]}" android:endY="{f["y1"]}">')
    else:
        g = (f'<gradient android:type="radial" android:centerX="{f["cx"]}" android:centerY="{f["cy"]}" '
             f'android:gradientRadius="{f["r"]}">')
    g += ''.join(f'<item android:offset="{o}" android:color="{c}"/>' for o, c in f['stops']) + '</gradient>'
    return '', f'<aapt:attr name="android:{attr}">{g}</aapt:attr>'

def bbox_lin(sh):
    # object-units gradients (handle): resolve to the path's own y extent
    import re
    ys = [float(v) for v in re.findall(r'[-\d.]+,([-\d.]+)', sh['d'])]
    xs = [float(v) for v in re.findall(r'([-\d.]+),[-\d.]+', sh['d'])]
    f = dict(sh['fill']); f.update(x0=min(xs), x1=max(xs), y0=0, y1=0)
    return f

def vd_path(sh):
    if 'group' in sh:
        inner = ''.join(vd_path(s) for s in sh['group'])
        return (f'<group android:rotation="{sh["rotate"]}" android:pivotX="{C}" android:pivotY="{MY}">'
                f'{inner}</group>')
    f = sh.get('fill')
    if sh.get('units') == 'obj': f = bbox_lin(sh)
    a, child = vd_fill('fillColor', f) if f else ('', '')
    if 'stroke' in sh:
        a += f' android:strokeColor="{sh["stroke"]}" android:strokeWidth="{sh["sw"]}"'
    if sh.get('evenodd'): a += ' android:fillType="evenOdd"'
    return f'<path android:pathData="{sh["d"]}"{a}>{child}</path>' if child else f'<path android:pathData="{sh["d"]}"{a}/>'

def write_vd(name, shapes, note):
    body = '\n    '.join(vd_path(s) for s in shapes)
    open(os.path.join(res, 'drawable', name), 'w').write(
        f'<?xml version="1.0" encoding="utf-8"?>\n<!-- {note} (generated by tools/icon/make_icon.py; drawn for this port) -->\n'
        '<vector xmlns:android="http://schemas.android.com/apk/res/android"\n'
        '    xmlns:aapt="http://schemas.android.com/aapt"\n'
        '    android:width="108dp" android:height="108dp"\n'
        '    android:viewportWidth="108" android:viewportHeight="108">\n    ' + body + '\n</vector>\n')

svg_defs, svg_n = [], [0]
def svg_paint(f, sh):
    if isinstance(f, str):
        c, a = argb_to_svg(f); return f'"{c}"', a
    svg_n[0] += 1; gid = f'g{svg_n[0]}'
    if sh.get('units') == 'obj': f = bbox_lin(sh)
    stops = ''.join(f'<stop offset="{o}" stop-color="{argb_to_svg(c)[0]}" stop-opacity="{argb_to_svg(c)[1]:.3f}"/>' for o, c in f['stops'])
    if f['kind'] == 'lin':
        svg_defs.append(f'<linearGradient id="{gid}" gradientUnits="userSpaceOnUse" x1="{f["x0"]}" y1="{f["y0"]}" '
                        f'x2="{f["x1"]}" y2="{f["y1"]}">{stops}</linearGradient>')
    else:
        svg_defs.append(f'<radialGradient id="{gid}" gradientUnits="userSpaceOnUse" cx="{f["cx"]}" cy="{f["cy"]}" '
                        f'r="{f["r"]}">{stops}</radialGradient>')
    return f'"url(#{gid})"', 1.0

def svg_path(sh):
    if 'group' in sh:
        return f'<g transform="rotate({sh["rotate"]} {C} {MY})">' + ''.join(svg_path(s) for s in sh['group']) + '</g>'
    out = f'<path d="{sh["d"]}"'
    if sh.get('fill'):
        p, a = svg_paint(sh['fill'], sh); out += f' fill={p} fill-opacity="{a:.3f}"'
    else: out += ' fill="none"'
    if 'stroke' in sh:
        c, a = argb_to_svg(sh['stroke']); out += f' stroke="{c}" stroke-opacity="{a:.3f}" stroke-width="{sh["sw"]}"'
    if sh.get('evenodd'): out += ' fill-rule="evenodd"'
    return out + '/>'

write_vd('ic_launcher_background.xml', bg, 'Launcher icon background: underwater light rays, bubbles, sand')
write_vd('ic_launcher_foreground.xml', fg, 'Launcher icon foreground: award ribbon with a fry spatula (safe zone r=33)')
write_vd('ic_launcher_monochrome.xml', mono, 'Launcher icon, themed (monochrome) layer: ribbon and spatula')

# preview: composed icon, adaptive circle mask, plus the monochrome layer on grey
body = ''.join(svg_path(s) for s in bg + fg)
mbody = ''.join(svg_path(s) for s in mono)
open(preview, 'w').write(
    '<svg xmlns="http://www.w3.org/2000/svg" width="660" height="230" viewBox="0 0 330 115">'
    f'<defs>{"".join(svg_defs)}<clipPath id="c"><circle cx="54" cy="54" r="50"/></clipPath>'
    '<clipPath id="s"><rect x="4" y="4" width="100" height="100" rx="22"/></clipPath></defs>'
    '<rect width="330" height="115" fill="#ECEFF1"/>'
    f'<g transform="translate(2,3)" clip-path="url(#c)">{body}</g>'
    f'<g transform="translate(112,3)" clip-path="url(#s)">{body}</g>'
    f'<g transform="translate(222,3)"><circle cx="54" cy="54" r="50" fill="#37474F"/>{mbody}</g></svg>')
print('ok')
