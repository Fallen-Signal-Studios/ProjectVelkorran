"""Save the visually reviewed assent-console replacement and verify reload."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_m13_assent_consoles.py'),init_globals={'SAVE_ASSENT_CONSOLES':True})
