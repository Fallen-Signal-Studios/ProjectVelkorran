"""Fresh-process readback and rendered inspection of the saved coffer mesh."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/refine_m13_coffer_readability.py'),init_globals={'READBACK_ONLY':True})
