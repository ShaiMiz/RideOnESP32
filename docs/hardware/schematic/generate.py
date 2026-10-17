"""Draws the RideOnESP32 draft schematic sheets as SVG.

Usage: python3 generate.py OUTDIR   (then ./render.sh OUTDIR for PNGs)
"""
import html
import os
import sys

ESC = html.escape
COL = dict(p24='#c62828', p5='#e65100', p3='#8d6e00', gnd='#212121', sig='#1565c0',
           i2c='#6a1b9a', tbd='#8a8a8a', box='#37474f', note='#455a64')
DATE = '2026-10-17'
TOTAL = 5


def cw(n, size=11):
    return n * size * 0.6


class S:
    def __init__(s, w, h, num, title):
        s.w, s.h, s.num, s.title = w, h, num, title
        s.e = []

    def add(s, x):
        s.e.append(x)

    def text(s, x, y, t, size=12, anchor='start', weight='normal', color='#212121', italic=False):
        st = ' font-style="italic"' if italic else ''
        s.add(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" '
              f'font-weight="{weight}" fill="{color}"{st}>{ESC(t)}</text>')

    def lines(s, x, y, ls, size=12, lh=None, **kw):
        lh = lh or round(size * 1.4)
        for i, l in enumerate(ls):
            bold = l.startswith('**')
            l = l.strip('*')
            s.text(x, y + i * lh, l, size, weight='bold' if bold else kw.get('weight', 'normal'),
                   color=kw.get('color', '#212121'), anchor=kw.get('anchor', 'start'))

    def wire(s, pts, c='sig', w=1.6, dash=None):
        color = COL.get(c, c)
        d = ' '.join(f'{x},{y}' for x, y in pts)
        da = f' stroke-dasharray="{dash}"' if dash else ''
        s.add(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="{w}" '
              f'stroke-linejoin="round" stroke-linecap="round"{da}/>')

    def dot(s, x, y, c='#212121'):
        s.add(f'<circle cx="{x}" cy="{y}" r="3.4" fill="{COL.get(c, c)}"/>')

    def rect(s, x, y, w, h, stroke='#37474f', fill='#ffffff', sw=1.4, rx=4, dash=None):
        da = f' stroke-dasharray="{dash}"' if dash else ''
        s.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
              f'stroke="{COL.get(stroke, stroke)}" stroke-width="{sw}"{da}/>')

    def part(s, x, y, w, h, title=None, left=(), right=(), top=(), bottom=(), fill='#f4f6f9',
             dash=None, tsize=12):
        s.rect(x, y, w, h, fill=fill, dash=dash)
        if title:
            s.text(x + w / 2, y - 7, title, tsize, 'middle', 'bold')
        pins = {}
        for name, py in left:
            s.text(x + 5, py + 4, name, 10.5)
            pins[name] = (x, py)
        for name, py in right:
            s.text(x + w - 5, py + 4, name, 10.5, 'end')
            pins[name] = (x + w, py)
        for name, px in top:
            s.text(px, y + 14, name, 10.5, 'middle')
            pins[name] = (px, y)
        for name, px in bottom:
            s.text(px, y + h - 6, name, 10.5, 'middle')
            pins[name] = (px, y + h)
        return pins

    def two(s, kind, p0, p1, ref='', val='', c='sig', w=1.6, side=1):
        (x0, y0), (x1, y1) = p0, p1
        horiz = y0 == y1
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        body = 24 if kind in ('cap', 'capE') else 30
        sgn = 1 if ((x1 > x0) if horiz else (y1 > y0)) else -1
        if horiz:
            a, b = (cx - sgn * body / 2, cy), (cx + sgn * body / 2, cy)
            ang = 0 if sgn > 0 else 180
        else:
            a, b = (cx, cy - sgn * body / 2), (cx, cy + sgn * body / 2)
            ang = 90 if sgn > 0 else -90
        s.wire([p0, a], c, w)
        s.wire([b, p1], c, w)
        k = '#212121'
        if kind == 'res':
            g = f'<rect x="-15" y="-6" width="30" height="12" fill="#fff" stroke="{k}" stroke-width="1.5"/>'
        elif kind == 'fuse':
            g = (f'<rect x="-15" y="-6" width="30" height="12" fill="#fff" stroke="{k}" stroke-width="1.5"/>'
                 f'<line x1="-15" y1="0" x2="15" y2="0" stroke="{k}" stroke-width="1.2"/>')
        elif kind in ('cap', 'capE'):
            g = (f'<line x1="-12" y1="0" x2="-3" y2="0" stroke="{k}" stroke-width="1.5"/>'
                 f'<line x1="3" y1="0" x2="12" y2="0" stroke="{k}" stroke-width="1.5"/>'
                 f'<line x1="-3" y1="-10" x2="-3" y2="10" stroke="{k}" stroke-width="2"/>')
            if kind == 'capE':
                g += (f'<path d="M5,-10 Q2,0 5,10" fill="none" stroke="{k}" stroke-width="2"/>'
                      f'<text x="-9" y="-6" font-size="10" fill="{k}" transform="rotate({-ang} -9 -9)">+</text>')
            else:
                g += f'<line x1="3" y1="-10" x2="3" y2="10" stroke="{k}" stroke-width="2"/>'
        elif kind == 'diode':
            g = (f'<line x1="-15" y1="0" x2="-8" y2="0" stroke="{k}" stroke-width="1.5"/>'
                 f'<polygon points="-8,-8 -8,8 7,0" fill="#fff" stroke="{k}" stroke-width="1.5"/>'
                 f'<line x1="7" y1="-8" x2="7" y2="8" stroke="{k}" stroke-width="2"/>'
                 f'<line x1="7" y1="0" x2="15" y2="0" stroke="{k}" stroke-width="1.5"/>')
        elif kind == 'sw':
            g = (f'<line x1="-15" y1="0" x2="-11" y2="0" stroke="{k}" stroke-width="1.5"/>'
                 f'<circle cx="-9" cy="0" r="2.2" fill="#fff" stroke="{k}" stroke-width="1.3"/>'
                 f'<circle cx="9" cy="0" r="2.2" fill="#fff" stroke="{k}" stroke-width="1.3"/>'
                 f'<line x1="-8" y1="-1" x2="10" y2="-10" stroke="{k}" stroke-width="1.7"/>'
                 f'<line x1="11" y1="0" x2="15" y2="0" stroke="{k}" stroke-width="1.5"/>')
        elif kind == 'swnc':
            g = (f'<line x1="-15" y1="0" x2="-11" y2="0" stroke="{k}" stroke-width="1.5"/>'
                 f'<circle cx="-9" cy="0" r="2.2" fill="#fff" stroke="{k}" stroke-width="1.3"/>'
                 f'<circle cx="9" cy="0" r="2.2" fill="#fff" stroke="{k}" stroke-width="1.3"/>'
                 f'<line x1="-8" y1="1" x2="12" y2="4.5" stroke="{k}" stroke-width="1.7"/>'
                 f'<line x1="11" y1="0" x2="15" y2="0" stroke="{k}" stroke-width="1.5"/>')
        s.add(f'<g transform="translate({cx},{cy}) rotate({ang})">{g}</g>')
        lab = ' '.join(t for t in (ref, val) if t)
        if lab:
            if horiz:
                s.text(cx, cy - 13, lab, 10.5, 'middle')
            elif side > 0:
                s.text(cx + 13, cy + 4, lab, 10.5)
            else:
                s.text(cx - 13, cy + 4, lab, 10.5, 'end')

    def gnd(s, x, y):
        k = '#212121'
        s.add(f'<g stroke="{k}" stroke-width="1.6"><line x1="{x}" y1="{y}" x2="{x}" y2="{y + 7}"/>'
              f'<line x1="{x - 9}" y1="{y + 7}" x2="{x + 9}" y2="{y + 7}"/>'
              f'<line x1="{x - 5.5}" y1="{y + 11}" x2="{x + 5.5}" y2="{y + 11}"/>'
              f'<line x1="{x - 2.5}" y1="{y + 15}" x2="{x + 2.5}" y2="{y + 15}"/></g>')

    def netlabel(s, x, y, name, d='right', c='sig'):
        color = COL.get(c, c)
        tw = cw(len(name), 10.5) + 10
        if d == 'right':
            pts = [(x, y), (x + 7, y - 8), (x + 7 + tw, y - 8), (x + 7 + tw, y + 8), (x + 7, y + 8)]
            tx, anchor = x + 9, 'start'
            ty = y + 4
        elif d == 'left':
            pts = [(x, y), (x - 7, y - 8), (x - 7 - tw, y - 8), (x - 7 - tw, y + 8), (x - 7, y + 8)]
            tx, anchor = x - 9, 'end'
            ty = y + 4
        elif d == 'up':
            pts = [(x, y), (x + tw / 2, y - 5), (x + tw / 2, y - 20), (x - tw / 2, y - 20), (x - tw / 2, y - 5)]
            tx, anchor, ty = x, 'middle', y - 8.5
        else:  # down
            pts = [(x, y), (x + tw / 2, y + 5), (x + tw / 2, y + 20), (x - tw / 2, y + 20), (x - tw / 2, y + 5)]
            tx, anchor, ty = x, 'middle', y + 16.5
        p = ' '.join(f'{a},{b}' for a, b in pts)
        s.add(f'<polygon points="{p}" fill="#ffffff" stroke="{color}" stroke-width="1.3"/>')
        s.text(tx, ty, name, 10.5, anchor, 'normal', color)

    def motor(s, x, y, r=22):
        s.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#fff" stroke="#212121" stroke-width="1.8"/>')
        s.text(x, y + 6, 'M', 17, 'middle', 'bold')

    def note(s, x, y, w, ls, title=None, kind='note', size=11.5):
        lh = round(size * 1.42)
        h = 16 + (len(ls) + (1 if title else 0)) * lh
        fill = '#fff8e1' if kind == 'tbd' else '#f1f5f9'
        stroke = '#b8860b' if kind == 'tbd' else '#90a4ae'
        s.rect(x, y, w, h, stroke=stroke, fill=fill, sw=1.2, rx=6, dash='5 3' if kind == 'tbd' else None)
        yy = y + 8 + lh - 3
        if title:
            s.text(x + 10, yy, title, size + 0.5, weight='bold')
            yy += lh
        for l in ls:
            bold = l.startswith('**')
            s.text(x + 10, yy, l.strip('*'), size, weight='bold' if bold else 'normal')
            yy += lh
        return y + h

    def header(s):
        s.text(30, 38, f'RideOnESP32 · Sheet {s.num} of {TOTAL} · {s.title}', 20, weight='bold')
        s.text(s.w - 30, 30, f'DRAFT v0.2 · {DATE}', 13, 'end', 'bold', '#c62828')
        s.text(s.w - 30, 47, 'Not for building until the TBD items are closed', 11, 'end', color='#c62828')
        s.add(f'<line x1="30" y1="54" x2="{s.w - 30}" y2="54" stroke="#cfd8dc" stroke-width="1"/>')

    def svg(s):
        s.header()
        body = '\n'.join(s.e)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{s.w}" height="{s.h}" '
                f'viewBox="0 0 {s.w} {s.h}" font-family="DejaVu Sans, Arial, Helvetica, sans-serif">\n'
                f'<rect x="0" y="0" width="{s.w}" height="{s.h}" fill="#ffffff"/>\n{body}\n</svg>\n')


# ---------------------------------------------------------------- sheet 1
def sheet1():
    s = S(1600, 1020, 1, 'Overview and plug-and-play interface')
    s.text(190, 86, 'CAR SIDE (stock, left untouched)', 12, 'middle', 'bold', '#546e7a')
    s.text(790, 86, 'NEW CONTROLLER BOX', 12, 'middle', 'bold', '#546e7a')
    s.text(1360, 86, 'OFF-BOX PARTS', 12, 'middle', 'bold', '#546e7a')

    car = [
        (110, 'Battery 24 V (Ah, chemistry TBD)', 'J1 BAT', 'p24', 3),
        (175, 'Key switch (stock dash)', 'J2 KEY', 'p24', 1.6),
        (290, 'Foot pedal (switch or pot: TBD)', 'J3 PEDAL', 'sig', 1.6),
        (355, 'Shifter F / R (+ speed switch?)', 'J4 SHIFT', 'sig', 1.6),
        (460, 'Steering motor (kid steers by hand)', 'J5 STEER', 'p24', 2.4),
        (530, 'Front motors FL, FR', 'J6 FRONT', 'p24', 3),
        (600, 'Rear motors RL, RR', 'J7 REAR', 'p24', 3),
        (690, 'Lights, horn, music (stock, TBD)', 'J8 ACC', 'tbd', 1.6),
    ]
    s.rect(480, 96, 620, 664, stroke='#37474f', fill='#fafbfc', sw=2.2, rx=10, dash='10 5')
    s.text(790, 752, 'Fits the stock controller spot under the seat · box size TBD (photos)', 11, 'middle',
           color='#546e7a', weight='bold')
    for y, label, j, c, w in car:
        s.rect(40, y, 300, 46, fill='#eef2f5')
        s.text(190, y + 28, label, 12, 'middle')
        s.wire([(340, y + 23), (480, y + 23)], c, w, '6 4' if c == 'tbd' else None)
        s.rect(480, y + 6, 84, 34, fill='#fff3e0', stroke='#e65100')
        s.text(522, y + 27, j, 11, 'middle', 'bold')
    s.text(410, 132 - 22, 'stock plugs', 10, 'middle', color='#546e7a')

    # inner blocks
    blocks = [
        (610, 110, 250, 150, 'Power and safety', 'sheet 2',
         ['F1 40 A main fuse', 'K1 contactor, Q1 hold MOSFET', 'Key → E-stop → K1 coil',
          'FB1 fuse block, U1–U4 rails']),
        (610, 290, 250, 150, 'Logic and sensors', 'sheet 4',
         ['ESP32 on MRD068A, ELRS RX', 'ADS1115 ×2, PCF8574, MPU-6050', 'PCA9685, DFPlayer, DS18B20',
          'pedal and shifter inputs']),
        (610, 460, 250, 180, 'Drive stage', 'sheets 3 and 5',
         ['4 independent drive channels', 'A: 4× IBT-2 (BTS7960)', 'B: 2× Cytron MDD20A',
          'Steering: 1× IBT-2', 'current sense per wheel']),
        (610, 670, 250, 60, 'Accessories', '', ['U29 MOSFET board, 12 V / 5 V rails']),
    ]
    for x, y, w, h, t, sh, ls in blocks:
        s.rect(x, y, w, h, fill='#ffffff', stroke='#37474f', sw=1.6, rx=6)
        s.text(x + 10, y + 20, t, 13, weight='bold')
        if sh:
            s.text(x + w - 10, y + 20, sh, 10.5, 'end', color='#1565c0')
        s.lines(x + 10, y + 40, ls, 11, 16)
    # car connectors to blocks
    s.wire([(564, 133), (610, 133)], 'p24', 3)
    s.wire([(564, 198), (610, 198)], 'p24', 1.6)
    s.wire([(564, 313), (610, 313)], 'sig', 1.6)
    s.wire([(564, 378), (610, 378)], 'sig', 1.6)
    s.wire([(564, 483), (610, 483)], 'p24', 2.4)
    s.wire([(564, 553), (610, 553)], 'p24', 3)
    s.wire([(564, 623), (610, 623)], 'p24', 3)
    s.wire([(564, 713), (610, 713)], 'tbd', 1.6, '6 4')
    # inter-block
    s.wire([(660, 260), (660, 290)], 'p5', 2)
    s.text(668, 279, '5 V, 3.3 V', 10, color='#e65100')
    s.wire([(700, 440), (700, 460)], 'sig', 2)
    s.text(708, 454, 'PWM ×10, EN ×2', 10, color='#1565c0')
    s.wire([(860, 180), (900, 180), (900, 560), (860, 560)], 'p24', 3)
    s.text(906, 250, 'MOTOR_24V', 10, color='#c62828')
    s.wire([(800, 640), (800, 670)], 'sig', 1.6)
    # right column
    right = [
        (125, 'E-stop S2, latching (dash)', 'J9 E-STOP', 'p24', 'optional 2nd E-stop at the rear'),
        (300, 'AS5600 steering angle', 'J10 SENS', 'i2c', 'on the steering column'),
        (365, 'Hall sensors ×2 (wheels)', 'J10 SENS', 'sig', 'A3144 + magnets'),
        (430, 'OLED dash, speaker', 'J11 DASH', 'i2c', 'later'),
    ]
    for y, label, j, c, sub in right:
        s.rect(1180, y, 360, 46, fill='#eef2f5')
        s.text(1360, y + 20, label, 12, 'middle')
        s.text(1360, y + 36, sub, 10, 'middle', color='#546e7a')
        s.rect(1056, y + 6, 84, 34, fill='#fff3e0', stroke='#e65100')
        s.text(1098, y + 27, j, 11, 'middle', 'bold')
        s.wire([(1140, y + 23), (1180, y + 23)], c, 1.6)
    s.wire([(1056, 148), (960, 148), (960, 150), (860, 150)], 'p24', 1.6)
    s.wire([(1056, 323), (860, 323)], 'i2c', 1.6)
    s.wire([(1056, 388), (860, 388)], 'sig', 1.6)
    s.wire([(1056, 453), (980, 453), (980, 425), (860, 425)], 'i2c', 1.6)
    # radio
    s.rect(1180, 540, 360, 64, fill='#e3f2fd', stroke='#1565c0')
    s.text(1360, 565, "Parent's radio: Jumper T-Lite V2", 12, 'middle', weight='bold')
    s.text(1360, 584, 'ELRS 2.4 GHz · CRSF · the only remote path', 10.5, 'middle')
    s.wire([(1180, 572), (1000, 572), (1000, 360), (860, 360)], 'sig', 1.6, '3 5')
    s.text(1010, 560, '2.4 GHz link to the receiver', 10, color='#1565c0')

    s.note(1180, 640, 360, [
        'Every plug on the box edge (J1–J8) is a mating',
        'connector for a stock plug, so the box swaps',
        'with the HY2005RX controller one-for-one.',
        'Connector types and pin counts: TBD from',
        'the photos of the stock controller.'], title='Stock plugs', kind='tbd')

    y = 800
    s.note(40, y, 700, [
        '1. Key off. Unplug J1–J8 from the new box.',
        '2. Plug the same stock plugs into the HY2005RX controller.',
        '3. If the stock harness joins the motors in pairs: the new box has its own',
        '   4-lead motor cable (J6, J7). Unplug each motor from it and plug it back',
        '   into its stock harness branch. Nothing is cut or spliced.',
        '4. Unplug the E-stop and sensors (J9–J11); the stock car has none.'],
        title='Going back to stock')
    # legend
    lx, ly = 800, 800
    s.rect(lx, ly, 760, 190, fill='#f1f5f9', stroke='#90a4ae', sw=1.2, rx=6)
    s.text(lx + 12, ly + 24, 'Legend (all sheets)', 12.5, weight='bold')
    items = [('p24', 4, None, 'Battery / motor current (thick = 10–14 AWG)'),
             ('p24', 1.6, None, '24 V switched, low current'),
             ('p5', 1.6, None, '5 V rails'), ('p3', 1.6, None, '3.3 V sensor rail'),
             ('gnd', 1.6, None, 'Ground'), ('sig', 1.6, None, 'Signal'),
             ('i2c', 1.6, None, 'I2C'), ('tbd', 1.6, '6 4', 'TBD / not confirmed')]
    for i, (c, w, da, t) in enumerate(items):
        col, row = i % 2, i // 2
        xx, yy = lx + 16 + col * 370, ly + 50 + row * 24
        s.wire([(xx, yy - 4), (xx + 50, yy - 4)], c, w, da)
        s.text(xx + 60, yy, t, 11)
    s.text(lx + 16, ly + 158, 'Net labels (flags) join wires with the same name across sheets.', 11)
    s.text(lx + 16, ly + 176, 'Sheets: 1 overview · 2 power and safety · 3 drive (IBT-2) · 4 logic · 5 variant B (MDD20A)', 11)
    return s


# ---------------------------------------------------------------- sheet 2
def sheet2():
    s = S(1600, 1150, 2, 'Power and safety chain')
    # battery
    s.part(60, 170, 110, 150, 'BT1 battery', right=[('+', 210), ('−', 290)])
    s.lines(70, 236, ['24 V nom.', 'chem. TBD', 'plug J1 TBD'], 10.5, 15)
    s.two('fuse', (170, 210), (270, 210), 'F1', '40 A', c='p24', w=4)
    s.text(220, 236, 'ANL / maxi, ≤15 cm', 9.5, 'middle', color='#546e7a')
    s.text(220, 248, 'from battery +', 9.5, 'middle', color='#546e7a')
    s.wire([(270, 210), (900, 210)], 'p24', 4)
    s.dot(300, 210)
    s.text(316, 200, 'BUS_F stud', 10.5, color='#c62828')
    s.text(620, 200, '10 AWG', 10.5, 'middle', color='#c62828')
    s.part(900, 160, 160, 120, 'K1 main contactor', left=[('30', 210)], right=[('87', 210)],
           bottom=[('86', 940), ('85', 1020)])
    s.text(980, 200, '24 V coil', 10.5, 'middle')
    s.text(980, 216, '≥80 A DC contacts', 10.5, 'middle')
    s.text(980, 232, 'or 80 A+ relay', 10.5, 'middle')
    s.wire([(1060, 210), (1240, 210)], 'p24', 4)
    s.netlabel(1240, 210, 'MOTOR_24V → sheet 3', 'right', 'p24')
    # battery minus
    s.wire([(170, 290), (480, 290)], 'gnd', 4)
    s.dot(230, 290)
    s.gnd(230, 290)
    s.text(60, 340, 'STAR GND at battery −', 10.5)
    s.netlabel(480, 290, 'MOTOR_GND → sheet 3', 'right', 'gnd')
    # key chain
    s.wire([(300, 210), (300, 330)], 'p24')
    s.two('fuse', (300, 330), (300, 390), 'F2', '15 A', c='p24')
    s.wire([(300, 390), (300, 420)], 'p24')
    s.two('sw', (300, 420), (300, 480), 'SW1', 'key switch (rating TBD)', c='p24')
    s.wire([(300, 480), (300, 520)], 'p24')
    s.dot(300, 520)
    s.text(292, 514, 'KEY_24V', 10.5, 'end', color='#c62828')
    # coil chain
    s.wire([(300, 520), (400, 520)], 'p24')
    s.two('fuse', (400, 520), (460, 520), 'F3', '1 A', c='p24')
    s.wire([(460, 520), (540, 520)], 'p24')
    s.two('swnc', (540, 520), (600, 520), 'S2a', 'NC', c='p24')
    s.wire([(600, 520), (940, 520), (940, 280)], 'p24')
    s.dot(940, 340)
    s.dot(1020, 340)
    s.two('diode', (1020, 340), (940, 340), 'D1', '1N4007', c='p24')
    s.text(980, 368, 'flyback', 10, 'middle', color='#546e7a')
    s.wire([(1020, 280), (1020, 560)], 'p24')
    s.part(980, 560, 150, 100, 'Q1 logic-level N-MOSFET', left=[('G', 610)], top=[('D', 1020)],
           bottom=[('S', 1020)])
    s.text(1070, 606, 'IRLZ44N', 10.5, 'middle')
    s.text(1070, 621, 'module', 10.5, 'middle')
    s.gnd(1020, 660)
    s.netlabel(760, 610, 'CONTACTOR_HOLD · GPIO12', 'left')
    s.wire([(760, 610), (790, 610)])
    s.two('res', (790, 610), (850, 610), 'R1', '100 Ω')
    s.wire([(850, 610), (980, 610)])
    s.dot(900, 610)
    s.two('res', (900, 610), (900, 680), 'R2', '10 k')
    s.gnd(900, 680)
    # E-stop sense contact
    s.netlabel(510, 690, 'ESTOP_SENSE → PCF8574 P3', 'left')
    s.wire([(510, 690), (540, 690)])
    s.two('sw', (540, 690), (600, 690), 'S2b', 'NO', c='sig')
    s.wire([(600, 690), (640, 690)], 'gnd')
    s.gnd(640, 690)
    s.wire([(570, 528), (570, 676)], '#757575', 1.2, '4 4')
    s.lines(312, 560, ['S2: one latching mushroom E-stop,', 'two contact blocks.',
                       'S2a NC opens the K1 coil;', 'S2b NO tells the ESP32 (0 = pressed)'], 10.5, 15)
    # fuse block
    s.wire([(300, 520), (300, 800)], 'p24')
    s.wire([(300, 800), (1340, 800)], 'p24', 3)
    s.text(316, 790, 'FB1 blade fuse block (KEY_24V bus) · spare ways for later', 10.5, color='#c62828')

    def buck(x, fref, fval, ref, l1, l2, out, oc):
        s.dot(x, 800)
        s.two('fuse', (x, 800), (x, 870), fref, fval, c='p24')
        s.wire([(x, 870), (x, 900)], 'p24')
        s.part(x - 60, 900, 120, 90, None, top=[('IN', x)], right=[('', 930)], bottom=[('GND', x)])
        s.text(x, 936, ref, 11, 'middle', 'bold')
        s.text(x, 951, l1, 10, 'middle')
        s.text(x, 964, l2, 10, 'middle')
        s.gnd(x, 990)

    buck(400, 'F4', '2 A', 'U1 LM2596', '24 → 5.0 V', '~0.5 A', '5V_LOGIC', 'p5')
    s.wire([(460, 930), (490, 930)], 'p5')
    s.dot(478, 930)
    s.netlabel(490, 930, '5V_LOGIC', 'right', 'p5')
    s.wire([(478, 930), (478, 1060), (500, 1060)], 'p5')
    s.part(500, 1032, 110, 56, None, left=[('IN', 1060)], right=[('OUT', 1060)])
    s.text(555, 1052, 'U2 3.3 V', 10.5, 'middle', 'bold')
    s.text(555, 1066, '1 A reg.', 10, 'middle')
    s.gnd(555, 1088)
    s.wire([(610, 1060), (640, 1060)], 'p3')
    s.netlabel(640, 1060, '3V3_SENSE', 'right', 'p3')
    buck(630, 'F5', '3 A', 'U3 buck', '24 → 5 V', '5 A', '5V_ACC', 'p5')
    s.wire([(690, 930), (705, 930)], 'p5')
    s.netlabel(705, 930, '5V_ACC', 'right', 'p5')
    buck(860, 'F6', '10 A', 'U4 buck', '24 → 12 V', '10–20 A, sealed', '12V_ACC', 'p5')
    s.wire([(920, 930), (935, 930)], 'p5')
    s.netlabel(935, 930, '12V_ACC', 'right', 'p5')
    s.dot(1090, 800)
    s.two('fuse', (1090, 800), (1090, 870), 'F7', '1 A', c='p24')
    s.wire([(1090, 870), (1090, 890)], 'p24')
    s.netlabel(1090, 890, 'FAN_24V', 'down', 'p24')
    s.text(1090, 930, 'box fans, switched', 10, 'middle', color='#546e7a')
    s.text(1090, 943, 'by U29 (sheet 4)', 10, 'middle', color='#546e7a')
    # battery divider
    x = 1320
    s.dot(x, 800)
    s.two('fuse', (x, 800), (x, 860), 'F8', '0.5 A', c='p24', side=-1)
    s.two('res', (x, 860), (x, 925), 'R3', '150 k', side=-1)
    s.wire([(x, 925), (x, 945)])
    s.dot(x, 945)
    s.two('res', (x, 945), (x, 1010), 'R4', '12 k', side=-1)
    s.gnd(x, 1010)
    s.wire([(x, 945), (1380, 945)])
    s.dot(1380, 945)
    s.two('cap', (1380, 945), (1380, 1005), 'C1', '100 nF')
    s.gnd(1380, 1005)
    s.wire([(1380, 945), (1400, 945)])
    s.netlabel(1400, 945, 'BATT_SENSE → GPIO39', 'right')
    s.text(1332, 1050, '2.18 V at 29.4 V', 10, color='#546e7a')

    ny = s.note(1180, 260, 390, [
        'Layer 1: key SW1 or E-stop S2a opens K1, no',
        'software involved.',
        'Layer 2: drivers default to off (enable lines',
        'pulled down, sheet 3).',
        'Layer 3: the ESP32 can only open K1 (Q1), never',
        'close it against the key or the E-stop. A reset,',
        'crash or brown-out drops GPIO12 and opens K1.',
        'After an E-stop the rails stay up, so the ESP32',
        'can report why it stopped. Key off kills all.'], title='How the chain works')
    s.note(1180, ny + 14, 390, [
        'SW1 carries the K1 coil plus every rail (~8 A',
        'worst case). If the stock key is rated lower,',
        'SW1 drives a 30 A relay that feeds FB1 instead.',
        'K1 inrush: 5 × 470 µF charge when K1 closes.',
        'Precharge resistor: TBD after the bench test.',
        'Rail fuses are sized for the 24 V input side.'], title='TBD', kind='tbd')
    return s


# ---------------------------------------------------------------- sheet 3
def ibt(s, ox, oy, n, title, pre, rp, lp, en, isn, mot, jref, pd):
    X = lambda v: ox + v
    Y = lambda v: oy + v
    s.part(X(340), Y(20), 180, 260, title,
           left=[('VCC', Y(50)), ('GND', Y(80)), ('RPWM', Y(110)), ('LPWM', Y(140)), ('R_IS', Y(170)),
                 ('L_IS', Y(200)), ('R_EN', Y(230)), ('L_EN', Y(260))],
           right=[('B+', Y(60)), ('B−', Y(110)), ('M+', Y(190)), ('M−', Y(240))])
    s.text(X(435), Y(132), 'IBT-2', 12, 'middle', 'bold')
    s.text(X(435), Y(148), 'BTS7960 ×2', 10.5, 'middle')
    s.text(X(435), Y(163), 'on-board IS R', 9.5, 'middle', color='#546e7a')
    s.text(X(435), Y(175), '(VERIFY, 1 k?)', 9.5, 'middle', color='#546e7a')
    s.wire([(X(340), Y(50)), (X(300), Y(50))], 'p3')
    s.netlabel(X(300), Y(50), '3V3_SENSE', 'left', 'p3')
    s.wire([(X(340), Y(80)), (X(310), Y(80))], 'gnd')
    s.gnd(X(310), Y(80))
    s.wire([(X(340), Y(110)), (X(300), Y(110))])
    s.netlabel(X(300), Y(110), f'{pre}_RPWM · GPIO{rp}', 'left')
    s.wire([(X(340), Y(140)), (X(300), Y(140))])
    s.netlabel(X(300), Y(140), f'{pre}_LPWM · GPIO{lp}', 'left')
    # current sense
    s.wire([(X(340), Y(170)), (X(290), Y(170)), (X(290), Y(200)), (X(340), Y(200))])
    s.dot(X(290), Y(185))
    s.wire([(X(290), Y(185)), (X(280), Y(185))])
    s.two('res', (X(280), Y(185)), (X(235), Y(185)), f'R{n}A', '4.7 k')
    s.wire([(X(235), Y(185)), (X(170), Y(185))])
    s.dot(X(222), Y(185))
    s.two('cap', (X(222), Y(185)), (X(222), Y(235)), '', '10 nF')
    s.gnd(X(222), Y(235))
    s.dot(X(170), Y(185))
    s.two('diode', (X(170), Y(185)), (X(170), Y(235)), f'D{n}', 'clamp', side=-1)
    s.netlabel(X(170), Y(235), '3V3_SENSE', 'down', 'p3')
    s.wire([(X(170), Y(185)), (X(135), Y(185))])
    s.netlabel(X(135), Y(185), isn, 'left')
    # enable
    s.wire([(X(340), Y(230)), (X(300), Y(230)), (X(300), Y(290))])
    s.wire([(X(340), Y(260)), (X(300), Y(260))])
    s.dot(X(300), Y(260))
    s.wire([(X(300), Y(290)), (X(135), Y(290))])
    s.netlabel(X(135), Y(290), en, 'left')
    if pd:
        s.dot(X(250), Y(290))
        s.two('res', (X(250), Y(290)), (X(250), Y(345)), f'R{n}B', '10 k pull-down')
        s.gnd(X(250), Y(345))
    else:
        s.text(X(140), Y(312), '(shared pull-down R10B on the FL board)', 9.5, color='#546e7a')
    # power side
    s.wire([(X(520), Y(60)), (X(680), Y(60))], 'p24', 2.6)
    s.netlabel(X(680), Y(60), 'MOTOR_24V', 'right', 'p24')
    s.wire([(X(520), Y(110)), (X(680), Y(110))], 'gnd', 2.6)
    s.netlabel(X(680), Y(110), 'MOTOR_GND', 'right', 'gnd')
    s.dot(X(580), Y(60))
    s.dot(X(580), Y(110))
    s.two('capE', (X(580), Y(60)), (X(580), Y(110)), f'C{n}B', '470 µF 35 V', c='p24')
    s.wire([(X(520), Y(190)), (X(620), Y(190))], 'p24', 2.2)
    s.wire([(X(520), Y(240)), (X(620), Y(240))], 'p24', 2.2)
    s.rect(X(620), Y(172), 36, 86, fill='#fff3e0', stroke='#e65100')
    s.text(X(638), Y(166), jref, 10.5, 'middle', 'bold')
    s.text(X(638), Y(194), '1', 10, 'middle')
    s.text(X(638), Y(244), '2', 10, 'middle')
    s.wire([(X(656), Y(190)), (X(700), Y(190)), (X(700), Y(193))], 'p24', 2.2)
    s.wire([(X(656), Y(240)), (X(700), Y(240)), (X(700), Y(237))], 'p24', 2.2)
    s.motor(X(700), Y(215))
    s.dot(X(700), Y(190))
    s.dot(X(700), Y(240))
    s.wire([(X(700), Y(190)), (X(745), Y(190))], 'p24')
    s.two('cap', (X(745), Y(190)), (X(745), Y(240)), '', '')
    s.wire([(X(745), Y(240)), (X(700), Y(240))], 'p24')
    s.text(X(705), Y(266), mot, 10.5, 'middle', 'bold')
    s.text(X(705), Y(280), '100 nF across terminals', 9.5, 'middle', color='#546e7a')
    s.text(X(705), Y(293), 'leads twisted, 12–14 AWG', 9.5, 'middle', color='#546e7a')


def sheet3():
    s = S(1600, 1200, 3, 'Drive stage, variant A: 4 × IBT-2 drive + 1 × IBT-2 steering')
    units = [
        (20, 70, 10, 'U10 · front-left drive', 'FL', 25, 26, 'DRIVE_EN · GPIO21', 'IS_FL → ADS#1 A0', 'M_FL',
         'J6.1', True),
        (810, 70, 11, 'U11 · front-right drive', 'FR', 27, 14, 'DRIVE_EN · GPIO21', 'IS_FR → ADS#1 A1', 'M_FR',
         'J6.2', False),
        (20, 430, 12, 'U12 · rear-left drive', 'RL', 32, 33, 'DRIVE_EN · GPIO21', 'IS_RL → ADS#1 A2', 'M_RL',
         'J7.1', False),
        (810, 430, 13, 'U13 · rear-right drive', 'RR', 13, 4, 'DRIVE_EN · GPIO21', 'IS_RR → ADS#1 A3', 'M_RR',
         'J7.2', False),
        (20, 790, 14, 'U14 · steering', 'STEER', 18, 19, 'STEER_EN · GPIO2', 'IS_ST → ADS#2 A0', 'M_STEER',
         'J5', True),
    ]
    for u in units:
        ibt(s, *u)
    s.wire([(800, 70), (800, 1150)], '#cfd8dc', 1)
    s.wire([(30, 420), (1570, 420)], '#cfd8dc', 1)
    s.wire([(30, 780), (1570, 780)], '#cfd8dc', 1)
    ny = s.note(830, 800, 740, [
        'B+ / B−: 10 AWG from K1 and the star point to a short bus, then 12–14 AWG to each board.',
        'R_IS and L_IS are joined: only the active half-bridge drives its IS pin. 4.7 k + clamp',
        'diode keep the ADS1115 input under 3.3 V; 10 nF (suggested) smooths the 16 kHz PWM.',
        'An IS pin pinned at the clamp (> 3.2 V) is the BTS7960 fault flag (config.h).',
        'IBT-2 VCC is 3.3 V so the ESP32 signals read as a clean HIGH. Never tie R_EN/L_EN high.',
        'Enable low = outputs off (high impedance): the motor coasts and the child can',
        'back-drive the steering by hand in Kid mode.',
        'IBT-2 logic GND joins B− on the board, so all grounds meet only at the star point.'],
        title='Notes')
    s.note(830, ny + 12, 740, [
        'J5, J6, J7: stock motor plug type and pin count. If the harness pairs the motors,',
        'J6/J7 become a new 4-lead cable to the motor plugs (see sheet 1, going back to stock).',
        'Motor and battery labels: the BTS7960 runs to 27.5 V; a full 7-cell lithium pack is 29.4 V.',
        'IS resistor value on the IBT-2 board sets IS_RESISTOR_OHMS in config.h.'],
        title='TBD (photos and labels)', kind='tbd')
    return s


# ---------------------------------------------------------------- sheet 4
def sheet4():
    s = S(1600, 1200, 4, 'Logic, radio and sensors')
    left = [('VIN', 160, '5V_LOGIC', 'p5'), ('GND', 190, None, 'gnd')]
    gp = [(25, 'FL_RPWM'), (26, 'FL_LPWM'), (27, 'FR_RPWM'), (14, 'FR_LPWM'), (32, 'RL_RPWM'),
          (33, 'RL_LPWM'), (13, 'RR_RPWM'), (4, 'RR_LPWM'), (18, 'STEER_RPWM'), (19, 'STEER_LPWM'),
          (21, 'DRIVE_EN'), (2, 'STEER_EN')]
    y = 220
    for g, n in gp:
        left.append((f'GPIO{g}', y, f'{n} (sheet 3)', 'sig'))
        y += 30
    left.append(('GPIO12', y, 'CONTACTOR_HOLD (sheet 2)', 'sig'))
    left.append(('GPIO39', y + 30, 'BATT_SENSE (sheet 2)', 'sig'))
    left.append(('3V3', y + 60, None, 'tbd'))
    right = [('GPIO16 RX2', 170), ('GPIO17 TX2', 202), ('GPIO22 SCL', 250), ('GPIO23 SDA', 282),
             ('GPIO36 ADC', 340), ('GPIO34 in', 440), ('GPIO35 in', 540), ('GPIO5', 610), ('GPIO15', 650)]
    s.part(620, 120, 260, 560, 'U20 ESP32-WROOM-32D 30-pin on MRD068A',
           left=[(a, b) for a, b, _, _ in left], right=right)
    s.text(750, 395, 'ESP32', 16, 'middle', 'bold')
    s.text(750, 414, 'DevKit, 30-pin', 11, 'middle')
    s.text(750, 430, 'pins from include/pins.h', 10, 'middle', color='#546e7a')
    for name, py, net, c in left:
        if name == 'GND':
            s.wire([(620, py), (592, py)], 'gnd')
            s.gnd(592, py)
        elif name == '3V3':
            s.wire([(620, py), (600, py)], 'tbd', 1.6, '4 3')
            s.text(596, py + 4, 'NC: do not join to 3V3_SENSE', 10.5, 'end', color='#757575')
        else:
            s.wire([(620, py), (590, py)], c)
            s.netlabel(590, py, net, 'left', c)
    # receiver
    s.part(1000, 100, 210, 125, 'U21 Jumper AION RX Mini (ELRS)',
           left=[('5V', 120), ('GND', 145), ('TX', 170), ('RX', 202)])
    s.text(1140, 150, 'CRSF', 11, 'middle', 'bold')
    s.text(1140, 165, '420 000 baud', 10, 'middle')
    s.text(1140, 180, 'UART2', 10, 'middle')
    s.wire([(1000, 120), (975, 120)], 'p5')
    s.netlabel(975, 120, '5V_LOGIC', 'left', 'p5')
    s.wire([(1000, 145), (985, 145)], 'gnd')
    s.gnd(985, 145)
    s.wire([(880, 170), (1000, 170)])
    s.wire([(880, 202), (1000, 202)])
    s.text(1222, 120, 'antenna outside any metal,', 10, color='#546e7a')
    s.text(1222, 133, 'vertical, away from motor wires', 10, color='#546e7a')
    s.wire([(880, 250), (905, 250)], 'i2c')
    s.netlabel(905, 250, 'SCL', 'right', 'i2c')
    s.wire([(880, 282), (905, 282)], 'i2c')
    s.netlabel(905, 282, 'SDA', 'right', 'i2c')
    # pedal
    s.wire([(880, 340), (1090, 340)])
    s.dot(930, 340)
    s.two('cap', (930, 340), (930, 390), 'C20', '100 nF')
    s.gnd(930, 390)
    s.dot(1020, 340)
    s.two('res', (1020, 340), (1020, 390), 'R21', '100 k')
    s.gnd(1020, 390)
    s.dot(1075, 340)
    s.two('diode', (1075, 340), (1075, 300), 'D20', '', side=-1)
    s.netlabel(1075, 300, '3V3_SENSE', 'up', 'p3')
    s.two('res', (1090, 340), (1170, 340), 'R20', '10 k')
    s.part(1170, 316, 190, 56, 'J3 stock pedal (type TBD)', left=[('SIG', 340)],
           right=[('V+', 330), ('GND', 356)], fill='#fff8e1', dash='5 3')
    s.wire([(1360, 330), (1385, 330)], 'p3')
    s.netlabel(1385, 330, '3V3_SENSE', 'right', 'p3')
    s.wire([(1360, 356), (1380, 356)], 'gnd')
    s.gnd(1380, 356)

    def hall(y, ref, side, n):
        s.wire([(880, y), (1150, y)])
        s.dot(1060, y)
        s.two('cap', (1060, y), (1060, y + 50), f'C{n}', '100 nF')
        s.gnd(1060, y + 50)
        s.dot(1110, y)
        s.two('res', (1110, y), (1110, y - 45), f'R{n}', '10 k', side=-1)
        s.netlabel(1110, y - 45, '3V3_SENSE', 'up', 'p3')
        s.part(1150, y - 18, 200, 50, f'{ref} A3144 hall · {side} wheel', left=[('OUT', y)],
               right=[('VCC', y - 5), ('GND', y + 18)])
        s.text(1220, y + 12, 'open collector', 9.5, 'middle', color='#546e7a')
        s.wire([(1350, y - 5), (1375, y - 5)], 'p5')
        s.netlabel(1375, y - 5, '5V_LOGIC', 'right', 'p5')
        s.wire([(1350, y + 18), (1370, y + 18)], 'gnd')
        s.gnd(1370, y + 18)

    hall(440, 'H1', 'left', 22)
    hall(540, 'H2', 'right', 23)
    s.wire([(880, 610), (905, 610)])
    s.netlabel(905, 610, 'SOUND_TX', 'right')
    s.wire([(880, 650), (905, 650)])
    s.netlabel(905, 650, 'ONEWIRE_TEMP', 'right')
    s.text(1000, 614, '→ U30 DFPlayer (below)', 10, color='#546e7a')
    s.text(1015, 654, '→ U31 DS18B20 (below)', 10, color='#546e7a')

    s.note(30, 120, 330, [
        'GPIO12 must be LOW at boot: R2',
        '10 k pull-down (sheet 2).',
        'GPIO2 must be LOW at boot: R14B',
        '(sheet 3). The board LED may sit',
        'on GPIO2 too; that is fine.',
        'GPIO5 and GPIO15 must be HIGH at',
        'boot: DFPlayer RX idles high, the',
        'DS18B20 pull-up holds GPIO15.',
        'GPIO14/15 can toggle during boot;',
        'harmless, DRIVE_EN is held low.',
        '34–39 are input only, no pull-ups.',
        'Analog on ADC1 only (32–39).',
        'GPIO36/39 glitch LOW ~80 ns on',
        'ADC power-up (errata): analog',
        'only, never pulse counting.',
        'VIN takes 5 V. The board 3V3 pin',
        'stays unconnected; sensors and',
        'IBT-2 logic use 3V3_SENSE (U2).',
        'All grounds are common, joined',
        'at the star point.'], title='Boot and strapping pins')

    # I2C panel
    s.rect(20, 690, 1560, 480, fill='#fbfaff', stroke='#b39ddb', sw=1.2, rx=8, dash='6 4')
    s.text(36, 712, 'I2C bus · 3V3_SENSE · 100–400 kHz · GPIO22 SCL, GPIO23 SDA', 12.5, weight='bold',
           color='#6a1b9a')
    s.wire([(60, 780), (1560, 780)], 'i2c', 2)
    s.wire([(60, 810), (1560, 810)], 'i2c', 2)
    s.netlabel(60, 780, 'SCL', 'left', 'i2c')
    s.netlabel(60, 810, 'SDA', 'left', 'i2c')
    s.dot(620, 780)
    s.dot(700, 810)
    s.two('res', (620, 780), (620, 738), 'R24', '4.7 k', c='i2c', side=-1)
    s.two('res', (700, 810), (700, 738), 'R25', '4.7 k', c='i2c')
    s.wire([(620, 738), (700, 738)], 'p3')
    s.netlabel(660, 738, '3V3_SENSE', 'up', 'p3')
    devs = [
        ('U22 ADS1115 #1', '0x48', ['VDD 3V3_SENSE', 'ADDR → GND', 'A0 IS_FL · A1 IS_FR', 'A2 IS_RL · A3 IS_RR',
                                    'ALRT: NC']),
        ('U23 ADS1115 #2', '0x49', ['VDD 3V3_SENSE', 'ADDR → VDD', 'A0 IS_ST', 'A1 AS5600 OUT (opt.)',
                                    'A2, A3 spare']),
        ('U24 PCF8574', '0x20', ['VCC 3V3_SENSE', 'A0–A2 → GND', 'P0–P4 switches below', 'P5–P7 spare',
                                 'INT: NC']),
        ('U25 AS5600', '0x36', ['VCC 3V3_SENSE', 'DIR → GND', 'steering column,', 'magnet on shaft',
                                'run length: see note']),
        ('U26 MPU-6050', '0x68', ['VCC 3V3_SENSE', 'AD0 → GND', 'fixed flat in box', 'tilt / crash (later)']),
        ('U27 SSD1306 OLED', '0x3C', ['VCC 3V3_SENSE', '(not 5 V: its pull-ups', 'would lift the bus)',
                                      'on dash (later)']),
        ('U28 PCA9685', '0x40', ['VCC 3V3_SENSE', 'OE → GND, V+ NC', 'CH0–7 → U29', 'accessories (later)']),
    ]
    for i, (t, a, ls) in enumerate(devs):
        x = 60 + i * 215
        s.dot(x + 50, 780)
        s.dot(x + 90, 810)
        s.wire([(x + 50, 780), (x + 50, 850)], 'i2c')
        s.wire([(x + 90, 810), (x + 90, 850)], 'i2c')
        bottom = [(f'P{k}', x + 20 + k * 37) for k in range(5)] if t.startswith('U24') else ()
        if t.startswith('U28'):
            bottom = [('CH0–7', x + 95)]
        s.part(x, 850, 190, 135, None, top=[('SCL', x + 50), ('SDA', x + 90)], bottom=bottom)
        s.text(x + 10, 884, t, 11.5, weight='bold')
        s.text(x + 180, 884, a, 11, 'end', color='#6a1b9a', weight='bold')
        s.lines(x + 10, 901, ls, 10.5, 14)
    # PCF switches
    px = 60 + 2 * 215
    for k in range(5):
        x = px + 20 + k * 37
        s.wire([(x, 985), (x, 1000)])
        s.two('sw', (x, 1000), (x, 1045), '', '')
        s.gnd(x, 1045)
    s.lines(px + 4, 1084, ['P0 forward · P1 reverse', 'P2 horn · P3 E-stop S2b', 'P4 seat (optional)',
                           'to GND, 0 = pressed', '1 k series on long leads'], 10.5, 14)
    # U29
    x = 60 + 6 * 215
    s.wire([(x + 95, 985), (x + 95, 1010)])
    s.part(x, 1010, 190, 150, None)
    s.lines(x + 10, 1030, ['**U29 MOSFET board', '4–8 ch, low-side', 'CH0 headlights 12V_ACC', 'CH1 tail 12V_ACC',
                           'CH2–3 fans FAN_24V', 'CH4+ spare', 'flyback diodes on fans', 'and any coil loads'],
            10.5, 15)
    s.part(60, 1010, 190, 150, None)
    s.lines(70, 1030, ['**U30 DFPlayer Mini', 'VCC 5V_ACC · GND', 'RX ← SOUND_TX via 1 k', 'SPK1/2 → LS1',
                       '3 in 4 Ω speaker', 'sounds on micro-SD'], 10.5, 15)
    s.part(275, 1010, 190, 150, None)
    s.lines(285, 1030, ['**U31 DS18B20 probe', 'DQ ← ONEWIRE_TEMP GPIO15', '4.7 k DQ → 3V3_SENSE',
                        'VDD 3V3_SENSE · GND', 'box temperature', 'proposed: GPIO15', 'was spare'], 10.5, 15)
    s.note(705, 1010, 620, [
        'Modules usually carry their own 4.7–10 k pull-ups. With seven on the bus they',
        'add up; remove extras so the total is about 2.2–4.7 k (R24/R25 are the only ones).',
        'AS5600 sits at the steering column, outside the box. If that run is over ~30 cm,',
        'read its analog OUT on ADS#2 A1 instead of I2C, or add a bus buffer (TBD).',
        'Long signal leads (pedal, shifter, halls): twisted with GND, locking plugs.'],
        title='I2C and cabling', kind='tbd')
    return s


# ---------------------------------------------------------------- sheet 5
def sheet5():
    s = S(1600, 1120, 5, 'Drive stage, variant B: 2 × Cytron MDD20A (compact, for the stock box)')
    # gate
    lp = []
    rp = []
    names = [('FL', 25, 26), ('FR', 27, 14), ('RL', 32, 33), ('RR', 13, 4)]
    for i, (n, pw, dr) in enumerate(names):
        ya = 170 + i * 60
        lp += [(f'{i + 1}A', ya), (f'{i + 1}B', ya + 20)]
        rp += [(f'{i + 1}Y', ya + 10)]
    s.part(330, 140, 150, 260, 'U40 74LVC08 quad AND', left=lp, right=rp, top=[('VCC', 405)],
           bottom=[('GND', 405)])
    s.text(405, 270, 'PWM', 11, 'middle', 'bold')
    s.text(405, 285, 'AND', 11, 'middle', 'bold')
    s.text(405, 300, 'DRIVE_EN', 11, 'middle', 'bold')
    s.wire([(405, 140), (405, 120)], 'p3')
    s.netlabel(405, 120, '3V3_SENSE', 'up', 'p3')
    s.gnd(405, 400)
    for i, (n, pw, dr) in enumerate(names):
        ya = 170 + i * 60
        s.wire([(330, ya), (305, ya)])
        s.netlabel(305, ya, f'{n}_PWM · GPIO{pw}', 'left')
        s.wire([(330, ya + 20), (305, ya + 20)])
        s.netlabel(305, ya + 20, 'DRIVE_EN', 'left')
        s.wire([(480, ya + 10), (505, ya + 10)])
        s.netlabel(505, ya + 10, f'{n}_PWM_G', 'right')
    # DRIVE_EN pull-down
    s.netlabel(170, 470, 'DRIVE_EN · GPIO21', 'left')
    s.wire([(170, 470), (230, 470)])
    s.dot(230, 470)
    s.two('res', (230, 470), (230, 530), 'R40', '10 k pull-down')
    s.gnd(230, 530)
    s.wire([(230, 470), (270, 470)])
    s.netlabel(270, 470, 'DRIVE_EN', 'right')
    s.text(40, 560, 'R40 replaces R10B (sheet 3) in this variant.', 10.5, color='#546e7a')

    def mdd(oy, ref, a, b, ia, ib):
        bx = 760
        s.part(bx, oy, 200, 320, f'{ref} Cytron MDD20A',
               left=[('GND', oy + 40), ('PWM1', oy + 80), ('DIR1', oy + 110), ('PWM2', oy + 160),
                     ('DIR2', oy + 190)],
               right=[('VB+', oy + 25), ('VB−', oy + 55), ('M1A', oy + 130), ('M1B', oy + 180),
                      ('M2A', oy + 230), ('M2B', oy + 280)])
        s.text(bx + 90, oy + 250, '2 × 20 A cont.', 10.5, 'middle')
        s.text(bx + 90, oy + 265, '60 A peak, 6–30 V', 10.5, 'middle')
        s.text(bx + 90, oy + 280, 'no EN, no IS pin', 10.5, 'middle', color='#c62828')
        s.wire([(bx, oy + 40), (bx - 25, oy + 40)], 'gnd')
        s.gnd(bx - 25, oy + 40)
        for (pin, net) in [('PWM1', f'{a[0]}_PWM_G'), ('DIR1', f'{a[0]}_DIR · GPIO{a[2]}'),
                           ('PWM2', f'{b[0]}_PWM_G'), ('DIR2', f'{b[0]}_DIR · GPIO{b[2]}')]:
            py = {'PWM1': 80, 'DIR1': 110, 'PWM2': 160, 'DIR2': 190}[pin] + oy
            s.wire([(bx, py), (bx - 25, py)])
            s.netlabel(bx - 25, py, net, 'left')
        r = bx + 200
        s.wire([(r, oy + 25), (r + 45, oy + 25)], 'p24', 2.6)
        s.netlabel(r + 45, oy + 25, 'MOTOR_24V', 'right', 'p24')
        s.wire([(r, oy + 55), (r + 45, oy + 55)], 'gnd', 2.6)
        s.netlabel(r + 45, oy + 55, 'MOTOR_GND', 'right', 'gnd')
        s.dot(r + 22, oy + 25)
        s.dot(r + 22, oy + 55)
        s.two('capE', (r + 22, oy + 25), (r + 22, oy + 55), '', '', c='p24')
        s.text(r + 140, oy + 44, '470 µF 35 V', 10, color='#546e7a')
        for (ya, yb, n, isn, j) in [(oy + 130, oy + 180, a[0], ia, a[3]), (oy + 230, oy + 280, b[0], ib, b[3])]:
            s.wire([(r, ya), (r + 30, ya)], 'p24', 2.2)
            s.part(r + 30, ya - 18, 90, 36, None, left=[('IP+', ya)], right=[('IP−', ya)], fill='#fff8e1',
                   dash='5 3')
            s.text(r + 75, ya + 4, 'CS', 10.5, 'middle', 'bold')
            s.wire([(r + 75, ya - 18), (r + 75, ya - 28)])
            s.netlabel(r + 75, ya - 28, isn, 'up')
            s.wire([(r + 120, ya), (r + 160, ya)], 'p24', 2.2)
            s.wire([(r, yb), (r + 160, yb)], 'p24', 2.2)
            s.rect(r + 160, ya - 14, 36, yb - ya + 28, fill='#fff3e0', stroke='#e65100')
            s.text(r + 178, ya - 19, j, 10.5, 'middle', 'bold')
            mid = (ya + yb) / 2
            mx = r + 245
            s.wire([(r + 196, ya), (mx, ya), (mx, mid - 22)], 'p24', 2.2)
            s.wire([(r + 196, yb), (mx, yb), (mx, mid + 22)], 'p24', 2.2)
            s.motor(mx, mid)
            s.text(mx + 30, mid + 4, f'M_{n}', 11, weight='bold')

    mdd(130, 'U41 front', ('FL', 25, 26, 'J6.1'), ('FR', 27, 14, 'J6.2'), 'IS_FL → ADS#1 A0',
        'IS_FR → ADS#1 A1')
    mdd(560, 'U42 rear', ('RL', 32, 33, 'J7.1'), ('RR', 13, 4, 'J7.2'), 'IS_RL → ADS#1 A2', 'IS_RR → ADS#1 A3')

    s.note(30, 600, 560, [
        'FL: PWM GPIO25, DIR GPIO26 · FR: PWM GPIO27, DIR GPIO14',
        'RL: PWM GPIO32, DIR GPIO33 · RR: PWM GPIO13, DIR GPIO4',
        'Same pins as variant A, new meaning: the drive class',
        'changes from RPWM/LPWM to PWM + DIR (firmware change).',
        'Steering stays on the U14 IBT-2 (sheet 3), so Kid-mode',
        'hand steering still coasts.'], title='Pin map, variant B')
    s.note(30, 790, 560, [
        'The MDD20A has no enable pin. GPIO14/15 toggle at boot',
        'and a crashed ESP32 can leave a PWM pin high, so U40',
        'forces every PWM input low unless DRIVE_EN (pulled',
        'down) is high. Layer 2 of the safety chain survives.',
        'VERIFY on the board: PWM low = brake (outputs low),',
        'so a disabled car brakes instead of coasting.'], title='Why the AND gate')
    ny = s.note(1300, 120, 280, [
        'No current-sense pin, so each',
        'channel needs its own sensor (CS)',
        'in one motor lead, e.g.',
        'ACS712-30A on 5V_LOGIC with a',
        '10 k / 20 k divider to the ADS,',
        'or a 3.3 V hall sensor (ACS724).',
        'Part and wiring TBD.'], title='Current sensing', kind='tbd')
    ny = s.note(1300, ny + 14, 280, [
        '+ 2 boards instead of 4: fits a',
        '  box the size of the stock one',
        '+ 3.3 V logic, test buttons',
        '− 20 A per wheel vs 43 A',
        '− needs 4 current sensors and',
        '  the AND gate',
        '− 30 V max: fine for 24 V lead-',
        '  acid or 7-cell lithium, no',
        '  margin above that'], title='Variant A vs B')
    s.note(1300, ny + 14, 280, [
        'Pick A or B after the photos',
        'and the stock box measurements.'], title='Decision', kind='tbd')
    return s


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for i, f in enumerate([sheet1, sheet2, sheet3, sheet4, sheet5], 1):
        sh = f()
        name = ['overview', 'power-safety', 'drive-ibt2', 'logic-sensors', 'drive-mdd20a'][i - 1]
        path = os.path.join(out, f'{i:02d}-{name}.svg')
        with open(path, 'w') as fh:
            fh.write(sh.svg())
        print(path, sh.w, sh.h)
