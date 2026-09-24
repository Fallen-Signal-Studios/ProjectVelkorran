"""Save the reviewed visual-only Z12 upper sidelights in M13."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
review_dir = root / 'Saved/Validation/Aurelion/Z12UpperSidelightPreview-20260924-022800-5caa9e1b'
review = json.loads((review_dir / 'upper-sidelight-preview.json').read_text())
assert review['status'] == 'unsaved_visual_preview'
assert review['removed_coffer_indices'] == [4, 5, 6, 7, 16, 17, 18, 19]
assert review['lower_service_registers_retained'] and review['native_wall_and_rib_collision_retained']
assert review['other_actor_state_retained'] and review['map_files_unchanged'] and review['visual_only']
assert all((review_dir / (side + '-upper-sidelight.png')).is_file() for side in ('west', 'east'))
source = root / 'Art/Source/Aurelion/Z12UpperSidelight'
manifest = json.loads((source / 'manifest.json').read_text())
assert json.loads((source / 'verification.json').read_text())['status'] == 'round_trip_pass'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert {s['asset']: digest(source / (s['asset'] + '.fbx')) for s in manifest['modules']} == review['mesh_source_sha256']
meshes = {s['asset']: unreal.load_asset(review['mesh_paths'][s['asset']]) for s in manifest['modules']}
assert all(isinstance(m, unreal.StaticMesh) for m in meshes.values())
sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for spec in manifest['modules']:
    mesh = meshes[spec['asset']]
    assert sm.get_num_uv_channels(mesh, 0) == 2
    assert sm.get_simple_collision_count(mesh) == sm.get_convex_collision_count(mesh) == 0
    assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve() == (source / (spec['asset'] + '.fbx')).resolve()
assert not sm.get_nanite_settings(meshes['SM_Aurelion_KIT_Z12UpperSidelightPane_4x3']).get_editor_property('enabled')

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
maps = {name: root / 'Content/Aurelion/Maps' / name for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
before = {name: digest(path) for name, path in maps.items()}
assert before == review['map_hashes_before']
existing = list(api.get_all_level_actors())
by_label = {a.get_actor_label(): a for a in existing}
owner = by_label['Aurelion_Art_M13_Z12_6_21a580']
side = next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
            if c.static_mesh and c.static_mesh.get_name() == 'SM_Aurelion_KIT_Z12CofferSide')
assert side.get_instance_count() == 24 and side.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
original = [side.get_instance_transform(i, world_space=True) for i in range(24)]
remove = [i for i, t in enumerate(original)
          if abs(abs(t.translation.x)-2100)<.01 and t.translation.y in (46900, 48100)
          and abs(t.translation.z-450)<.01]
assert remove == review['removed_coffer_indices']
register_labels = ['PREVIEW_Aurelion_Z12_ServiceRegister_%02d' % i for i in (4, 5, 8, 9, 20, 21, 24, 25)]
assert all(by_label[name].static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           for name in register_labels)
for label in ('Z12_Wall_EW-1', 'Z12_Wall_EW1', 'Z12_Rib_1_-1', 'Z12_Rib_1_1'):
    comp = by_label[label].static_mesh_component
    assert not comp.get_editor_property('visible')
    assert comp.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
others = helper['snapshot_actor_state']([a for a in existing if a != owner])
assert not any(label.replace('PREVIEW_', '', 1) in by_label for label in review['placed_labels'])
side.modify()
side.clear_instances()
for i, t in enumerate(original):
    if i not in remove:
        side.add_instance(t, world_space=True)
assert side.get_instance_count() == 16
assert [side.get_instance_transform(i, world_space=True).export_text() for i in range(16)] == [
    t.export_text() for i, t in enumerate(original) if i not in remove]
created = []
for direction, x in (('West', -2100), ('East', 2100)):
    for station, y in (('Aft', 46900), ('Fore', 48100)):
        for role, mesh in (('Frame', meshes['SM_Aurelion_KIT_Z12UpperSidelightFrame_4x3']),
                           ('Pane', meshes['SM_Aurelion_KIT_Z12UpperSidelightPane_4x3'])):
            actor = api.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, 450))
            assert actor
            actor.set_actor_label('Z12_%sUpperSidelight_%s_%s' % (direction, station, role))
            actor.set_folder_path('Aurelion/Z12/UpperSidelights')
            actor.set_actor_rotation(unreal.Rotator(yaw=-90 if x < 0 else 90), True)
            comp = actor.static_mesh_component
            comp.set_static_mesh(mesh)
            comp.set_collision_profile_name('NoCollision')
            comp.set_editor_property('can_ever_affect_navigation', False)
            if role == 'Pane':
                comp.set_cast_shadow(False)
            actor.set_actor_enable_collision(False)
            created.append(actor)
assert [a.get_actor_label() for a in created] == [s.replace('PREVIEW_', '', 1) for s in review['placed_labels']]
assert helper['snapshot_actor_state']([a for a in existing if a != owner]) == others
assert all(a.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           and not a.static_mesh_component.get_editor_property('can_ever_affect_navigation') for a in created)
assert {name: digest(path) for name, path in maps.items()} == before
backup = out / 'L_Aurelion_M13.before.umap'
shutil.copy2(maps['L_Aurelion_M13.umap'], backup)
assert level.save_current_level()
after = {name: digest(path) for name, path in maps.items()}
assert after['L_Aurelion_M12.umap'] == before['L_Aurelion_M12.umap']
assert after['L_Aurelion_M13.umap'] != before['L_Aurelion_M13.umap']
(out / 'upper-sidelight-save.json').write_text(json.dumps(dict(
    status='saved_requires_fresh_editor_verification', reviewed_preview=str(review_dir),
    removed_visual_coffer_indices=remove, retained_lower_service_register_labels=register_labels,
    retained_native_wall_and_rib_collision=True, visual_only=True,
    new_actor_labels=[a.get_actor_label() for a in created],
    mesh_paths=review['mesh_paths'], mesh_source_sha256=review['mesh_source_sha256'],
    map_hashes_before=before, map_hashes_after=after, map_backup=str(backup),
    all_other_actor_state_retained=True, new_collision_and_nav_disabled=True), indent=2))
print('M13_UPPER_SIDELIGHT_SAVED_REQUIRES_FRESH_EDITOR_VERIFICATION')
