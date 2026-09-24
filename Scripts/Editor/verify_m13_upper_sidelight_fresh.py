"""Fresh-editor verification of saved M13 upper sidelights and player-eye views."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
save_dir = root / 'Saved/Validation/Aurelion/Z12UpperSidelightSave-20260924-023042-e7cb2eaf'
saved = json.loads((save_dir / 'upper-sidelight-save.json').read_text())
assert saved['status'] == 'saved_requires_fresh_editor_verification'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
maps = {name: root / 'Content/Aurelion/Maps' / name for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
assert {name: digest(path) for name, path in maps.items()} == saved['map_hashes_after']
actors = list(api.get_all_level_actors())
by_label = {a.get_actor_label(): a for a in actors}
assert len(by_label) == len(actors)
owner = by_label['Aurelion_Art_M13_Z12_6_21a580']
side = next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
            if c.static_mesh and c.static_mesh.get_name() == 'SM_Aurelion_KIT_Z12CofferSide')
assert side.get_instance_count() == 16 and side.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
poses = [side.get_instance_transform(i, world_space=True) for i in range(16)]
assert not any(abs(abs(p.translation.x)-2100)<.01 and p.translation.y in (46900, 48100)
               and abs(p.translation.z-450)<.01 for p in poses)
register_labels = saved['retained_lower_service_register_labels']
assert len(register_labels) == 8
assert all(by_label[label].static_mesh_component.get_editor_property('visible')
           and by_label[label].static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           for label in register_labels)
for label in ('Z12_Wall_EW-1', 'Z12_Wall_EW1', 'Z12_Rib_1_-1', 'Z12_Rib_1_1'):
    comp = by_label[label].static_mesh_component
    assert not comp.get_editor_property('visible')
    assert comp.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
labels = saved['new_actor_labels']
assert len(labels) == 8 and all(sum(a.get_actor_label() == label for a in actors) == 1 for label in labels)
for direction, x in (('West', -2100), ('East', 2100)):
    for station, y in (('Aft', 46900), ('Fore', 48100)):
        for role, mesh_key in (('Frame', 'SM_Aurelion_KIT_Z12UpperSidelightFrame_4x3'),
                               ('Pane', 'SM_Aurelion_KIT_Z12UpperSidelightPane_4x3')):
            actor = by_label['Z12_%sUpperSidelight_%s_%s' % (direction, station, role)]
            loc = actor.get_actor_location()
            assert max(abs(a-b) for a,b in zip((loc.x, loc.y, loc.z), (x, y, 450))) < .1
            comp = actor.static_mesh_component
            assert comp.static_mesh.get_path_name() == saved['mesh_paths'][mesh_key]
            assert comp.get_editor_property('visible')
            assert comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
            assert not comp.get_editor_property('can_ever_affect_navigation')
            assert not actor.get_actor_enable_collision()
            if role == 'Pane':
                assert not comp.get_editor_property('cast_shadow')
assert not any('UpperSidelight_' in label and label.startswith('PREVIEW_') for label in by_label)
(out / 'upper-sidelight-fresh.json').write_text(json.dumps(dict(
    status='fresh_editor_loaded_pass', saved_map_sha256=saved['map_hashes_after']['L_Aurelion_M13.umap'],
    frames=4, panes=4, retained_lower_service_registers=8, retained_native_wall_and_rib_collision=True,
    visual_actors_no_collision_or_navigation=True, actor_count=len(actors)), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'M13_ROUTE_VIEWS': [('west-final-sidelight', (-900, 47200, 180)),
                        ('east-final-sidelight', (900, 47800, 180))],
    'M13_ROUTE_YAWS': {'west-final-sidelight': 180, 'east-final-sidelight': 0},
    'M13_ROUTE_PITCHES': {'west-final-sidelight': 8, 'east-final-sidelight': 8},
})
print('M13_UPPER_SIDELIGHT_FRESH_PASS')
