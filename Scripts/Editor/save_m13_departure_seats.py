"""Save the visually reviewed custom departure seating and verify its reload."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
runpy.run_path(str(root/'Scripts/Editor/fit_m13_departure_seats.py'),init_globals={
    'SAVE_DEPARTURE_SEATS':True,
    'REVIEWED_DEPARTURE_SEATS':str(root/'Saved/Validation/Aurelion/DepartureSeatPreview-20260920-233355-52950f29/departure-seat-fit.json')})
