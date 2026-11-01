#!/usr/bin/env python3
"""Procedural rack faces for the 1.7 POST expansion, the 1.8 motion/space models,
the 1.9 CREATIVE slot, and the 1.10 SYNTH page's engine strips.

Original Chimera fascias drawn from simple shapes with the working name; no
manufacturer logo or trade dress. Live controls stay JUCE components; these
images are the decorative strip behind the POST rack header.
Run: python3 Tools/generate_post_rack_faces.py [name ...]   (no names: all faces)
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
W, H = 1300, 360
FACES = [
    ('rpolar', 'POLAR PRESS', (36, 38, 44), (210, 120, 60), 'dial'),
    ('rthrust', 'THRUST BUS', (58, 64, 72), (220, 226, 232), 'meters'),
    ('rdiode', 'DIODE BRIDGE', (78, 86, 96), (196, 64, 52), 'meters'),
    ('rdiscrete', 'DISCRETE LIFT', (40, 44, 52), (226, 210, 120), 'slim'),
    ('rdesk', 'DESK TWO', (92, 98, 92), (238, 236, 226), 'meters'),
    ('rclear', 'CLEAR LINE', (172, 176, 180), (24, 28, 32), 'slim'),
    ('rstepped', 'STEPPED EQ', (34, 36, 40), (90, 160, 220), 'slim'),
    ('rair', 'AIR BAND', (210, 206, 196), (40, 92, 160), 'dial'),
    ('rcurve', 'CURVE SHAPER', (60, 46, 40), (232, 196, 120), 'meters'),
    # 1.8 motion/space
    ('rtapcloud', 'TAP CLOUD', (30, 52, 60), (130, 210, 230), 'slim'),
    ('rbloom', 'BLOOM DELAY', (96, 52, 78), (250, 200, 220), 'dial'),
    ('rdualdig', 'DUAL DIGITAL', (34, 48, 66), (200, 220, 240), 'meters'),
    ('rhallprime', 'HALL PRIME', (44, 50, 70), (210, 216, 236), 'slim'),
    ('rvoid', 'VOID SPACE', (18, 14, 30), (170, 120, 240), 'dial'),
    ('rgated', 'GATED ROOM', (96, 82, 54), (36, 30, 20), 'meters'),
    ('rspin', 'SPIN CABINET', (110, 76, 46), (240, 222, 190), 'dial'),
    ('rthrob', 'THROB VIBE', (70, 40, 86), (240, 200, 120), 'dial'),
    ('rring', 'RING BELL', (86, 86, 40), (20, 20, 20), 'slim'),
    # 1.9 CREATIVE
    ('cslicer', 'GATE SLICER', (92, 40, 52), (246, 214, 220), 'steps'),
    ('charmonizer', 'STEP HARMONIZER', (44, 54, 98), (214, 222, 250), 'steps'),
    ('ctimebend', 'TIME BEND', (88, 76, 44), (244, 226, 170), 'dial'),
    ('czap', 'ZAP SWEEP', (30, 70, 64), (140, 240, 210), 'wave'),
    ('cfreqshift', 'FREQ SHIFT', (62, 40, 86), (220, 190, 250), 'wave'),
    ('cfreeze', 'FREEZE HOLD', (38, 62, 92), (200, 230, 250), 'slim'),
    ('cglitch', 'GLITCH STUTTER', (98, 62, 30), (250, 214, 170), 'steps'),
    ('cbitcrush', 'BIT CRUSH', (62, 82, 44), (210, 240, 160), 'steps'),
    ('cgrain', 'GRAIN CLOUD', (82, 50, 82), (240, 200, 240), 'dial'),
]


def face(name, label, body, accent, motif):
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 8, W - 1, H - 9), 10, fill=body + (255,))
    for x in (30, W - 30):   # mounting ears with slotted screw holes
        for y in (60, H - 60):
            d.rounded_rectangle((x - 14, y - 8, x + 14, y + 8), 8, fill=(18, 18, 20, 255))
    for y in range(20, H - 20, 4):   # brushed finish
        shade = tuple(min(255, c + (5 if y % 8 else -3)) for c in body)
        d.line((60, y, W - 60, y), fill=shade + (255,))
    d.rectangle((60, 30, W - 60, 34), fill=accent + (255,))
    d.rectangle((60, H - 34, W - 60, H - 30), fill=accent + (255,))
    font = ImageFont.truetype(FONT, 44)
    d.text((90, 60), label, font=font, fill=accent + (255,))
    if motif == 'meters':   # VU windows, kept to the left third
        for i in range(2):
            x0 = 90 + i * 190
            d.rounded_rectangle((x0, 140, x0 + 170, 300), 8, fill=(236, 224, 190, 255), outline=(20, 20, 20, 255), width=4)
            d.arc((x0 + 20, 170, x0 + 150, 300), 200, 340, fill=(30, 30, 30, 255), width=3)
            d.line((x0 + 85, 290, x0 + 120, 190), fill=(160, 30, 20, 255), width=3)
    elif motif == 'dial':   # one large central dial outline
        d.ellipse((150, 140, 300, 290), outline=accent + (255,), width=6)
        d.ellipse((205, 195, 245, 235), fill=accent + (255,))
    elif motif == 'wave':   # oscilloscope window with a trace
        d.rounded_rectangle((90, 140, 420, 300), 10, fill=(14, 20, 18, 255), outline=accent + (255,), width=4)
        import math
        points = [(100 + x, 220 - 55 * math.sin(x / 26.0) * math.exp(-x / 420.0)) for x in range(0, 310, 2)]
        d.line(points, fill=accent + (255,), width=4)
    elif motif == 'steps':   # step-sequencer lamps
        for i in range(8):
            x0 = 100 + i * 44
            lit = i % 3 != 1
            d.rounded_rectangle((x0, 200, x0 + 30, 260), 6, fill=(accent if lit else tuple(c // 3 for c in accent)) + (255,))
    else:   # slim module lines
        for i in range(6):
            d.rectangle((100 + i * 60, 150, 140 + i * 60, 300), outline=accent + (200,), width=3)
    d.ellipse((W - 120, 60, W - 96, 84), fill=(220, 60, 40, 255) if body[0] < 150 else (60, 140, 220, 255))
    img.save(ROOT / 'Assets/Artwork' / f'{name}.png', optimize=True)


# 1.10 SYNTH engine strips (docs/SYNTH_V2.md): a wide, low faceplate behind
# the SYNTH page's top bar, one per engine type, named at its right end.
SW, SH = 2800, 56
STRIPS = [
    ('sdvergr', 'DVERGR ANALOG', (54, 40, 30), (240, 168, 72), 'forge'),
    ('sseidr', 'SEIDR DYNAMIC', (28, 42, 58), (150, 214, 236), 'weave'),
    ('smimir', 'MIMIR IMPACT', (34, 30, 48), (214, 196, 250), 'strike'),
]


def strip(name, label, body, accent, motif):
    import math
    img = Image.new('RGBA', (SW, SH), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, SW - 1, SH - 1), 8, fill=body + (255,))
    for y in range(2, SH - 2, 3):   # brushed finish
        shade = tuple(min(255, max(0, c + (4 if y % 6 else -3))) for c in body)
        d.line((6, y, SW - 6, y), fill=shade + (255,))
    dim = tuple(int(c * .55 + b * .45) for c, b in zip(accent, body))
    if motif == 'forge':   # rivets and glowing vent slots, like a warm valve chassis
        for x in range(40, SW - 520, 120):
            d.ellipse((x - 5, 10, x + 5, 20), fill=dim + (255,))
            d.ellipse((x - 5, SH - 20, x + 5, SH - 10), fill=dim + (255,))
            d.rounded_rectangle((x + 30, 18, x + 90, SH - 18), 6, fill=tuple(int(c * .35) for c in accent) + (255,), outline=dim + (255,), width=2)
    elif motif == 'weave':   # interlaced waves: a playing-dependent sound
        for k, phase in enumerate((0, 2.1, 4.2)):
            points = [(x, SH / 2 + (14 - 3 * k) * math.sin(x / (60 + 25 * k) + phase)) for x in range(10, SW - 520, 6)]
            d.line(points, fill=(dim if k else accent) + (255,), width=3 if k == 0 else 2)
    else:   # concentric rings struck from points: a struck sound
        for cx in range(80, SW - 560, 260):
            for r, c in ((22, accent), (15, dim), (8, dim)):
                d.ellipse((cx - r * 2, SH / 2 - r, cx + r * 2, SH / 2 + r), outline=c + (255,), width=2)
            d.ellipse((cx - 4, SH / 2 - 4, cx + 4, SH / 2 + 4), fill=accent + (255,))
    d.rectangle((SW - 500, 8, SW - 497, SH - 8), fill=accent + (255,))
    font = ImageFont.truetype(FONT, 30)
    d.text((SW - 480, (SH - 34) // 2), label, font=font, fill=accent + (255,))
    img.save(ROOT / 'Assets/Artwork' / f'{name}.png', optimize=True)


if __name__ == '__main__':
    import sys
    wanted = set(sys.argv[1:])
    for f in FACES:
        if not wanted or f[0] in wanted:
            face(*f)
    for f in STRIPS:
        if not wanted or f[0] in wanted:
            strip(*f)
