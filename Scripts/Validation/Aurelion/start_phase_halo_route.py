"""Normal campaign input route with passive protection and companion observers."""
from pathlib import Path
import runpy
here=Path(__file__).resolve().parent
runpy.run_path(str(here/'observe_lethal_floor_route.py'))
runpy.run_path(str(here/'start_companion_mesh_route.py'))
