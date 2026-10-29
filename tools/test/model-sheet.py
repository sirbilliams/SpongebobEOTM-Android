#!/usr/bin/env python3
"""Label the captures from omt1_models_test; needs ImageMagick, no game art is bundled.
Usage: tools/test/model-sheet.py .shots/models
"""
import pathlib
import subprocess
import sys

out = pathlib.Path(sys.argv[1])
args = ['montage', '-font', 'DejaVu-Sans', '-pointsize', '12']
for row in (out / 'thumbnails.tsv').read_text().splitlines():
    filename, model = row.split('\t')
    args += ['-label', pathlib.Path(model).name, str(out / filename)]
args += ['-tile', '8x', '-geometry', '240x180+2+2', str(out / 'contact.png')]
subprocess.run(args, check=True)
