"""Persist the visually reviewed chamber floor replacement only."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_m13_chamber_floor.py'),init_globals={'SAVE_CHAMBER_FLOOR':True})
