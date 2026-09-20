"""Save only the visually reviewed M13 centerpiece and verify the reloaded map."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_m13_crownmark_core.py'),
              init_globals={'SAVE_CROWNMARK_CORE':True})
