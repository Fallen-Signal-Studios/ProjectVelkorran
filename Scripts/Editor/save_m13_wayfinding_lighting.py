"""Save the visually reviewed sign light wash to M13 only, then reload/verify."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('refine_m13_wayfinding_lighting.py')),
              init_globals={'SAVE_WAYFINDING_LIGHTING': True})
