"""Inventory saved companion presentation defaults, then observe the normal route."""
from pathlib import Path
import runpy

here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'inspect_companion_mesh_defaults.py'))
runpy.run_path(str(here.parents[1] / 'Editor' / 'start_companion_route_observed.py'))
