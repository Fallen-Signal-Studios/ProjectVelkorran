"""Verify saved M13 keys through earned public reload, without modifying any lights."""
from pathlib import Path
import runpy,unreal
root=Path(unreal.Paths.project_dir())
saved=root/'Saved/Validation/Aurelion/DepartureKeySave-20260920-221326-f6af4ce2/departure-keys-saved.json'
runpy.run_path(str(Path(__file__).with_name('preview_m13_departure_keys.py')),init_globals={'VERIFY_SAVED_KEYS':str(saved)})
