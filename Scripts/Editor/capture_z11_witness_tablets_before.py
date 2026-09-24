"""Fixed player-height baseline of the five Z11 native request visuals."""
from pathlib import Path
import os
import runpy

import unreal

root = Path(unreal.Paths.project_dir())
base = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
out = base / 'Before'
out.mkdir(parents=True, exist_ok=True)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
old = os.environ['SOV_AURELION_RUN_DIRECTORY']
try:
    os.environ['SOV_AURELION_RUN_DIRECTORY'] = str(out)
    runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
        'M13_ROUTE_VIEWS': [
            ('witness-room', (750,43100,190)),
            ('witness-near', (100,42830,155)),
            ('witness-close', (360,43330,155)),
        ],
        'M13_ROUTE_YAWS': {'witness-room':180,'witness-near':180,'witness-close':180},
        'M13_ROUTE_PITCHES': {'witness-room':-8,'witness-near':-10,'witness-close':-11},
    })
finally:
    os.environ['SOV_AURELION_RUN_DIRECTORY'] = old
print('Z11_WITNESS_BASELINE_CAPTURE_STARTED', out)
