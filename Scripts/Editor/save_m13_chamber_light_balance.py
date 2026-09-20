"""Save only the reviewed M13 local-light balance and verify its reload."""
from pathlib import Path
import runpy,unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_m13_chamber_light_balance.py'),init_globals={'SAVE_LIGHT_BALANCE':True})
