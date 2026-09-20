"""Persist the reviewed ring kit and verify the scoped M13 map readback."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_m13_ceiling_rings.py'),init_globals={'SAVE_CEILING_RINGS':True})
