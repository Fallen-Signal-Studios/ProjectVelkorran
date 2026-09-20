"""Persist the reviewed local rail and destination-sign materials."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/refine_m13_readability.py'),init_globals={'SAVE_M13_READABILITY':True})
