from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z02_gallery.py').read_text(encoding='utf-8-sig'),'verify_z02_gallery','exec'))
assert len(actors)==2035+z02_guard_count+z02_bridges_count+z02_exterior_count+z02_facade_light_count+north_guard_count+north_bridge_count+atrium_guard_count+atrium_ring_count+atrium_parapet_count and z02_furniture_count==30
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z02_furniture.py'))['check_furniture'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z02-furniture-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
