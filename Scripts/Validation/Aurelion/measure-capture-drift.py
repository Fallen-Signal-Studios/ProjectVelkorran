"""Measure differences between fixed-camera PNGs; never infer art acceptance."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image

parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);parser.add_argument('pairs',nargs='+',help='name.png:name.png');args=parser.parse_args()
rows=[]
for pair in args.pairs:
    left,right=pair.split(':');a=np.asarray(Image.open(args.directory/left).convert('RGB'),dtype=np.float32);b=np.asarray(Image.open(args.directory/right).convert('RGB'),dtype=np.float32)
    assert a.shape==b.shape
    delta=np.abs(a-b);peak=delta.max(axis=2)
    rows.append(dict(left=left,right=right,mean_abs_rgb_255=float(delta.mean()),p95_max_channel_255=float(np.percentile(peak,95)),fraction_pixels_over_8=float((peak>8).mean()),mean_rgb_left=float(a.mean()),mean_rgb_right=float(b.mean())))
result=dict(status='measured',pairs=rows,qualification='Raw pixel differences only; exposure, geometry and final quality require visual interpretation.')
(args.directory/'capture-drift.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
