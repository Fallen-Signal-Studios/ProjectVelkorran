"""Fresh-load checks of the authored bridge and prior Z01 architectural pieces."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z01_paving.py').read_text(encoding='utf-8-sig'),'verify_z01_paving','exec'))
assert len(actors)==1915+endwall_count+z02_paving_count+z02_vault_count+z02_perimeter_count+z02_gallery_count+z02_furniture_count+z02_guard_count+z02_bridges_count+z02_exterior_count+z02_facade_light_count+north_guard_count+north_bridge_count+atrium_guard_count+atrium_ring_count+atrium_parapet_count+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count+z06_gate_assembly_count+z06_flank_landing_count+z07_paving_count+z07_wall_count+z07_light_count
bridge=labels['KIT_Z01_StoneBridge']; p=bridge.get_actor_location(); scale=bridge.get_actor_scale3d()
assert abs(p.x+6164.686518)<.01 and abs(p.y+14700)<.01 and abs(p.z)<.01
assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z))
assert abs(bridge.get_actor_rotation().yaw)<.01
c=bridge.static_mesh_component; mesh=c.static_mesh
assert mesh.get_name()=='SM_Aurelion_KIT_Z01_StoneBridge'
assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
assert bridge.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
assert str(c.get_collision_profile_name())=='BlockAll'
spec=json.loads((root/'Art/Source/Aurelion/BridgeKit/manifest.json').read_text())['modules'][0]
assert sm.get_convex_collision_count(mesh)==spec['convex_hulls'] and sm.get_simple_collision_count(mesh)==0
assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
old=labels['ramp'].static_mesh_component
assert not old.get_editor_property('visible') and old.get_editor_property('hidden_in_game')
assert str(old.get_collision_profile_name())=='NoCollision' and old.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
checks=runpy.run_path(str(root/'Scripts/Editor/check_z01_bridge_geometry.py'))['check_bridge'](world,bridge,spec,[a for a in actors if a!=bridge])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'bridge-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),checks=checks,
    qualification='Authored geometry and isolated collision; live movement, navigation, combat cover and performance unqualified'),indent=2))
