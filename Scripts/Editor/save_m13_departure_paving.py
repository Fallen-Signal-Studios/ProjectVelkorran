"""Save the reviewed departure paving and verify its exact level reload."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
runpy.run_path(str(root/'Scripts/Editor/fit_m13_departure_paving.py'),init_globals={
    'SAVE_DEPARTURE_PAVING':True,
    'REVIEWED_DEPARTURE_PAVING':str(root/'Saved/Validation/Aurelion/DeparturePavingOverview-20260921-000135-48b29240/departure-paving-fit.json')})
