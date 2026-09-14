"""Fresh-load architecture regression plus the actual eastbound turn."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_north_bridges.py').read_text(),'verify_north_bridges','exec'))
turn=runpy.run_path(str(root/'Scripts/Editor/check_atrium_bridge_exit.py'))['check_exit'](world,True)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'atrium-bridge-exit-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),turn_controls=turn,qualification='Whole-world collision sweeps and retained floor fit; live player/companion traversal remains unqualified.'),indent=2))
