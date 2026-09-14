"""Read-only fresh-load checks for Z02 paving and all completed Z01 assemblies."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z01_cover.py').read_text(encoding='utf-8-sig'),'verify_z01_cover','exec'))
assert len(actors)==1974+z02_vault_count+z02_perimeter_count+z02_gallery_count+z02_furniture_count+z02_guard_count+z02_bridges_count+z02_exterior_count+z02_facade_light_count+north_guard_count+north_bridge_count+atrium_guard_count+atrium_ring_count+atrium_parapet_count+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count+z06_gate_assembly_count+z06_flank_landing_count+z07_paving_count+z07_wall_count and z02_paving_count==45
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z02_paving.py'))['check_paving'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z02-paving-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
