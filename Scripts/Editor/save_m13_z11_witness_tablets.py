"""Save only the already reviewed Z11 witness-tablet visual replacement."""
from pathlib import Path
import os
import runpy

import unreal

root = Path(unreal.Paths.project_dir())
base = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
review = base / 'Preview/z11-witness-tablet-fit.json'
assert review.is_file()
os.environ['SOV_Z11_TABLET_SAVE'] = '1'
os.environ['SOV_Z11_TABLET_REVIEW'] = str(review)
runpy.run_path(str(root / 'Scripts/Editor/fit_m13_z11_witness_tablets.py'))
