"""Persist the visually reviewed custom chamber railings only."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_m13_railings.py'),init_globals={'SAVE_M13_RAILS':True})
