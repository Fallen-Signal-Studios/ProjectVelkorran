"""Persist the inspected departure coffer preview in M13 only."""
from pathlib import Path
import runpy,unreal
root=Path(unreal.Paths.project_dir())
runpy.run_path(str(Path(__file__).with_name('fit_m13_departure_walls.py')),init_globals={
    'SAVE_DEPARTURE_WALLS':True,
    'REVIEWED_DEPARTURE_FIT':str(root/'Saved/Validation/Aurelion/DepartureCofferPreview-20260920-224809-2ba00a13/departure-wall-fit.json')})
