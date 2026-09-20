"""Save only the reviewed M13 sign housings and verify the reloaded map."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_m13_wayfinding_portals.py'),
              init_globals={'SAVE_WAYFINDING':True})
