"""Fresh-load checks for the vault replacement; does not simulate gameplay."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z01_lower_architecture.py').read_text(encoding='utf-8-sig'),'verify_z01_lower_architecture','exec'))
assert len(actors)==1804+uplight_count+paving_count+bridge_count+endwall_count+z02_paving_count+z02_vault_count+z02_perimeter_count+z02_gallery_count+z02_furniture_count+z02_guard_count+z02_bridges_count+z02_exterior_count+z02_facade_light_count+north_guard_count+north_bridge_count+atrium_guard_count+atrium_ring_count+atrium_parapet_count+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count+z06_gate_assembly_count+z06_flank_landing_count+z07_paving_count
for label in ('KIT_Z01_UpperEnclosure','things'):
    c=labels[label].static_mesh_component
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
checked=[]
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for label,a in labels.items():
    if not label.startswith(('KIT_Z01_Vault_','KIT_Z01_VaultEnd_')): continue
    p=a.get_actor_location(); scale=a.get_actor_scale3d()
    assert abs(p.x+7010)<.01 and abs(p.z-695)<.01
    assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z))
    if label.startswith('KIT_Z01_Vault_'):
        index=int(label.rsplit('_',1)[1]); expected_y=-17300+400*index
        assert abs(a.get_actor_rotation().yaw)<.01
        mesh_name='SM_Aurelion_KIT_Vault_25x4'
    else:
        index=int(label.rsplit('_',1)[1]); expected_y=(-17480,-11920)[index]
        assert abs(abs(a.get_actor_rotation().yaw)-(180 if index else 0))<.01
        mesh_name='SM_Aurelion_KIT_VaultTympanum_25m'
    assert abs(p.y-expected_y)<.01
    c=a.static_mesh_component; mesh=c.static_mesh
    assert mesh.get_name()==mesh_name
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
    checked.append(label)
assert len(checked)==16
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'vault-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),
    placements=checked,qualification='Geometry, visibility and import settings; lighting, live traversal and performance remain pending'),indent=2))
