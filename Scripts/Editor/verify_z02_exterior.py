from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z02_bridges.py').read_text(encoding='utf-8-sig'),'verify_z02_bridges','exec'))
assert len(actors)==2062+z02_facade_light_count+north_guard_count and z02_exterior_count==13
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z02_exterior.py'))['check_exterior'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z02-exterior-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
