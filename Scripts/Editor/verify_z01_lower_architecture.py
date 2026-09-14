"""Read-only fresh-load check of the saved Z01 wall replacement."""
import json
import os
from pathlib import Path
import unreal

editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M12'
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
labels = {a.get_actor_label(): a for a in actors}
vault_count = sum(a.get_actor_label().startswith(('KIT_Z01_Vault_', 'KIT_Z01_VaultEnd_')) for a in actors)
assert vault_count in (0, 16), 'Partial or duplicate vault assembly'
uplight_count = sum(a.get_actor_label().startswith('KIT_Z01_Uplight_') for a in actors)
assert uplight_count in (0, 12), 'Partial or duplicate uplight assembly'
paving_count = sum(a.get_actor_label().startswith('KIT_Z01_Paving_') for a in actors)
assert paving_count in (0, 98), 'Partial or duplicate paving assembly'
bridge_count = sum(a.get_actor_label()=='KIT_Z01_StoneBridge' for a in actors)
assert bridge_count in (0, 1), 'Duplicate bridge assembly'
endwall_count = sum(a.get_actor_label().startswith('KIT_Z01_Endwall_') for a in actors)
assert endwall_count in (0, 14), 'Partial or duplicate end-wall assembly'
z02_paving_count = sum(a.get_actor_label().startswith('KIT_Z02_Paving_') for a in actors)
assert z02_paving_count in (0, 45), 'Partial or duplicate Z02 paving assembly'
z02_vault_count = sum(a.get_actor_label().startswith('KIT_Z02_Vault_') for a in actors)
assert z02_vault_count in (0, 13), 'Partial or duplicate Z02 vault assembly'
z02_perimeter_count = sum(a.get_actor_label().startswith('KIT_Z02_Perimeter_') for a in actors)
assert z02_perimeter_count in (0, 12), 'Partial or duplicate Z02 perimeter assembly'
z02_gallery_count = sum(a.get_actor_label().startswith('KIT_Z02_Gallery_') for a in actors)
assert z02_gallery_count in (0, 6), 'Partial or duplicate Z02 gallery assembly'
z02_furniture_count = sum(a.get_actor_label().startswith('KIT_Z02_Furniture_') for a in actors)
assert z02_furniture_count in (0, 30), 'Partial or duplicate Z02 furniture assembly'
z02_guard_count = sum(a.get_actor_label().startswith('KIT_Z02_ApproachGuard_South_') for a in actors)
assert z02_guard_count in (0, 10), 'Partial or duplicate approach guard assembly'
z02_bridges_count = sum(a.get_actor_label().startswith('KIT_Z02_Bridge_') for a in actors)
assert z02_bridges_count in (0, 4), 'Partial or duplicate Z02 bridge assembly'
z02_exterior_count = sum(a.get_actor_label().startswith('KIT_Z02_Exterior_') for a in actors)
assert z02_exterior_count in (0, 13), 'Partial or duplicate Z02 exterior assembly'
z02_facade_light_count = sum(a.get_actor_label().startswith('KIT_Z02_FacadeLight_') for a in actors)
assert z02_facade_light_count in (0, 12), 'Partial or duplicate Z02 facade lighting'
north_guard_count = sum(a.get_actor_label().startswith('KIT_North_ApproachGuard_') for a in actors)
assert north_guard_count in (0, 30), 'Partial or duplicate north connector parapets'
north_bridge_count = sum(a.get_actor_label().startswith('KIT_North_Bridge_') for a in actors)
assert north_bridge_count in (0, 4), 'Partial or duplicate north bridge assembly'
atrium_guard_count = sum(a.get_actor_label().startswith('KIT_Atrium_ApproachGuard_') for a in actors)
assert atrium_guard_count in (0, 34), 'Partial or duplicate atrium approach parapets'
atrium_ring_count = sum(a.get_actor_label().startswith('KIT_Atrium_Ring_') for a in actors)
assert atrium_ring_count in (0, 64), 'Partial or duplicate atrium ring floors'
atrium_parapet_count = sum(a.get_actor_label().startswith('KIT_Atrium_Parapet_') for a in actors)
assert atrium_parapet_count in (0, 266), 'Partial or duplicate atrium parapets'
atrium_canopy_count = sum(a.get_actor_label().startswith('KIT_Atrium_Canopy_') for a in actors)
assert atrium_canopy_count in (0, 8), 'Partial or duplicate atrium canopies'
atrium_bridge_floor_count = sum(a.get_actor_label().startswith('KIT_Atrium_BridgeFloor_') for a in actors)
assert atrium_bridge_floor_count in (0, 4), 'Partial or duplicate atrium bridge floors'
atrium_crown_count = sum(a.get_actor_label().startswith('KIT_Atrium_Crown_') for a in actors)
assert atrium_crown_count in (0, 9), 'Partial or duplicate atrium crown'
atrium_crown_light_count = sum(a.get_actor_label().startswith('KIT_Atrium_CrownLight_') for a in actors)
assert atrium_crown_light_count in (0, 16), 'Partial or duplicate crown lighting'
z06_paving_count = sum(a.get_actor_label().startswith('KIT_Z06_Paving_') for a in actors)
assert z06_paving_count in (0, 126), 'Partial or duplicate Z06 paving'
z06_ceiling_count = sum(a.get_actor_label().startswith('KIT_Z06_Ceiling_') for a in actors)
assert z06_ceiling_count in (0, 126), 'Partial or duplicate Z06 ceiling'
z06_side_count = sum(a.get_actor_label().startswith('KIT_Z06_Side_') for a in actors)
assert z06_side_count in (0, 36), 'Partial or duplicate Z06 side walls'
z06_light_count = sum(a.get_actor_label().startswith('KIT_Z06_Uplight_') for a in actors)
assert z06_light_count in (0, 16), 'Partial or duplicate Z06 uplighting'
assert len(actors) == 1788 + vault_count + uplight_count + paving_count + bridge_count + endwall_count + z02_paving_count + z02_vault_count + z02_perimeter_count + z02_gallery_count + z02_furniture_count + z02_guard_count + z02_bridges_count + z02_exterior_count + z02_facade_light_count + north_guard_count + north_bridge_count + atrium_guard_count + atrium_ring_count + atrium_parapet_count + atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count, 'Unexpected actor additions or removals'
shell = labels['aurelionwalls']
assert shell.get_actor_enable_collision()
assert not shell.static_mesh_component.get_editor_property('visible')
assert shell.static_mesh_component.get_editor_property('hidden_in_game')
assert shell.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
upper = labels['KIT_Z01_UpperEnclosure']
assert upper.get_actor_transform().export_text() == shell.get_actor_transform().export_text()
origin, extent = upper.get_actor_bounds(False)
assert abs(origin.z - extent.z - 695) < .2
assert not upper.get_actor_enable_collision()
assert upper.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
placed = []
for side, x, yaw in (('West', -8300, -90), ('East', -5700, 90)):
    proxy = labels['Z01_Wall_EW' + ('-1' if side == 'West' else '1')]
    p = proxy.get_actor_location()
    assert abs(p.x - x) < .1 and abs(p.y + 14700) < .1 and abs(p.z - 300) < .1
    assert proxy.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
    for name, count in (('WallPlain_4x7', 14), ('Pier_1x7', 13)):
        for i in range(count):
            label = f'KIT_Z01_{side}_{name}_{i:02}'
            actor = labels[label]
            scale = actor.get_actor_scale3d()
            assert all(abs(v - 1) < .001 for v in (scale.x, scale.y, scale.z))
            assert abs(actor.get_actor_rotation().yaw - yaw) < .01
            assert not actor.get_actor_enable_collision()
            assert actor.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
            assert actor.static_mesh_component.get_editor_property('visible')
            assert actor.static_mesh_component.static_mesh.get_name() == 'SM_Aurelion_KIT_' + name
            placed.append(label)
assert len(placed) == 54
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
(out / 'z01-reload-verification.json').write_text(json.dumps(dict(
    status='passed', actor_count=len(actors), lower_wall_placements=placed,
    upper_shell_bottom_cm=origin.z-extent.z,
    qualification='Saved geometry and collision settings only; live traversal and gameplay remain unqualified'
), indent=2))
