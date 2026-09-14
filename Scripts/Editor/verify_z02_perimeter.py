"""Fresh-load perimeter verification and prior assembly checks."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z02_vault.py').read_text(encoding='utf-8-sig'),'verify_z02_vault','exec'))
assert len(actors)==1999+z02_gallery_count+z02_furniture_count+z02_guard_count+z02_bridges_count+z02_exterior_count+z02_facade_light_count+north_guard_count+north_bridge_count+atrium_guard_count+atrium_ring_count+atrium_parapet_count+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count and z02_perimeter_count==12
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z02_perimeter.py'))['check_perimeter'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z02-perimeter-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
runpy.run_path(str(root/'Scripts/Editor/audit_z02_fittings.py'))
