"""Fresh-load north bridges, physical joins and previous architecture assemblies."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_north_guards.py').read_text(),'verify_north_guards','exec'))
assert len(actors)==2108+atrium_guard_count+atrium_ring_count+atrium_parapet_count+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count and north_bridge_count==4
checks=runpy.run_path(str(root/'Scripts/Editor/check_z02_bridges.py'))
geometry=checks['check_bridges'](world,actors,'NorthBridgeKit','KIT_North_Bridge_')
geometry['route_controls']=checks['route_controls'](world,actors,('bridge4','bridge5'))
assert all(row['blocker'] is None for row in geometry['route_controls'])
geometry['seams']=runpy.run_path(str(root/'Scripts/Editor/check_north_bridge_seams.py'))['check_seams'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'north-bridge-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
