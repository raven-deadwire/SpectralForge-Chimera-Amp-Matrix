#!/usr/bin/env python3
"""Procedural rack faces for the 1.7 POST expansion (models 3-5 per section).

Original Chimera fascias drawn from simple shapes with the working name; no
manufacturer logo or trade dress. Live controls stay JUCE components; these
images are the decorative strip behind the POST rack header.
Run: python3 Tools/generate_post_rack_faces.py
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
    else:   # slim module lines
        for i in range(6):
            d.rectangle((100 + i * 60, 150, 140 + i * 60, 300), outline=accent + (200,), width=3)
    d.ellipse((W - 120, 60, W - 96, 84), fill=(220, 60, 40, 255) if body[0] < 150 else (60, 140, 220, 255))
    img.save(ROOT / 'Assets/Artwork' / f'{name}.png', optimize=True)


if __name__ == '__main__':
    for f in FACES:
        face(*f)
