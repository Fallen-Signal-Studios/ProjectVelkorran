"""Save and verify the reviewed M13 support module placement."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_m13_chamber_ribs.py'),init_globals={'SAVE_CHAMBER_RIBS':True})
