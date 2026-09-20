"""Normal campaign route with camera API boundary checks and passive observers."""
from pathlib import Path
import runpy

here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'check_camera_perspective_runtime.py'))
runpy.run_path(str(here / 'observe_camera_route.py'))
runpy.run_path(str(here / 'start_companion_mesh_route.py'))
