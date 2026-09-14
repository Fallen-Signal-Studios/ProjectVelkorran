"""Fresh-load architecture regression including all fitted ring floor sectors."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_atrium_guards.py').read_text(),'verify_atrium_guards','exec'))
assert len(actors)==2206 and atrium_ring_count==64
geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_rings.py'))['check_rings'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'atrium-ring-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
