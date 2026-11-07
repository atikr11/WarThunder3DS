"""Approximate desktop layout previews from actual generated triangles.
These verify UI layout, not the 3DS GPU's rotation, clipping, or performance.
"""
from pathlib import Path
import struct
from PIL import Image, ImageDraw
root = Path(__file__).resolve().parent / 'output'
for name in ['menu', 'tank', 'pause', 'result', 'air', 'destroyed', 'zoom']:
    data = (root / (name + '.bin')).read_bytes()
    records = list(struct.iter_unpack('<16f', data))
    images = []
    for screen, width in [(0,400),(1,320)]:
        image = Image.new('RGB',(width*4,240*4), (140,183,204) if not screen else (11,20,32))
        draw = ImageDraw.Draw(image)
        records0 = [r for r in records if int(r[0]) == screen]
        world = sorted((r for r in records0 if r[1]), key=lambda r:sum([r[4],r[7],r[10]])/3, reverse=True)
        ui = [r for r in records0 if not r[1]]
        for r in world+ui:
            xy = [(r[2],r[3]),(r[5],r[6]),(r[8],r[9])]
            if any(abs(a)>100000 or abs(b)>100000 for a,b in xy):continue
            color = tuple(max(0,min(255,round(c*255))) for c in r[11:14])
            draw.polygon([(a*4,b*4) for a,b in xy], fill=color)
        images.append(image.resize((width,240),Image.Resampling.NEAREST))
    combined = Image.new('RGB', (420, 508), (4,8,12))
    combined.paste(images[0], (10,6)); combined.paste(images[1], (50,258))
    combined.resize((840,1016),Image.Resampling.NEAREST).save(root/(name+'.png'))
print('Wrote six desktop layout previews')
