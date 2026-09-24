"""Apply a reviewed Z12 canopy plan to M13 and verify a fresh editor reload."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion/Z12DepartureCanopy'
review_dir = root / 'Saved/Validation/Aurelion/Z12CanopyPlacementReview-20260923-184909-cf8415cf'
review = json.loads((review_dir / 'canopy-preview.json').read_text())
manifest = json.loads((source / 'manifest.json').read_text())
placement = runpy.run_path(str(root / 'Scripts/Editor/aurelion_z12_canopy_plan.py'))
rows = placement['plan'](manifest)
assert review['status'] == 'unsaved_preview' and review['instances'] == 102
assert review['placement_sha256'] == placement['digest'](rows)
assert review['map_actor_state_preserved'] and review['no_preview_collision']
assert review['source_fbx_sha256'] == {
    spec['asset']: hashlib.sha256((source / (spec['asset'] + '.fbx')).read_bytes()).hexdigest()
    for spec in manifest['modules']}
assert all((review_dir / (name + '.png')).is_file() for name in (
    'dominion-arrival', 'reformation-arrival', 'dominion-through-view', 'reformation-through-view'))

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
existing = list(actors.get_all_level_actors())
baseline = helper['snapshot_actor_state'](existing)
assert not any(a.get_actor_label() in {r['label'] for r in rows} for a in existing)
maps = {name: root / 'Content/Aurelion/Maps' / name
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name: digest(path) for name, path in maps.items()}
assert before == review['map_hashes_before']

meshes = {spec['asset']: unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/' + spec['asset'])
          for spec in manifest['modules']}
assert all(meshes.values())
assert {key: mesh.get_path_name() for key, mesh in meshes.items()} == review['mesh_paths']
created = []
for row in rows:
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*row['location']))
    assert actor
    actor.set_actor_label(row['label'])
    actor.set_folder_path('Aurelion/Z12/Canopy')
    component = actor.static_mesh_component
    component.set_static_mesh(meshes[row['asset']])
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    created.append(actor)
assert len(created) == 102
assert all(a.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           and not a.static_mesh_component.get_editor_property('can_ever_affect_navigation')
           for a in created)
assert helper['snapshot_actor_state'](existing) == baseline
assert {name: digest(path) for name, path in maps.items()} == before

backup = out / 'L_Aurelion_M13.before.umap'
shutil.copy2(maps['L_Aurelion_M13.umap'], backup)
assert level.save_current_level()
assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
reloaded = list(actors.get_all_level_actors())
bylabel = {a.get_actor_label(): a for a in reloaded}
assert all(sum(a.get_actor_label() == row['label'] for a in reloaded) == 1 for row in rows)
assert helper['snapshot_actor_state']([a for a in reloaded if a.get_actor_label() not in
                                      {r['label'] for r in rows}]) == baseline
for row in rows:
    actor = bylabel[row['label']]
    mesh = actor.static_mesh_component.static_mesh
    loc = actor.get_actor_location()
    assert mesh.get_path_name() == meshes[row['asset']].get_path_name()
    assert max(abs(v - target) for v, target in zip((loc.x, loc.y, loc.z), row['location'])) < .1
    assert actor.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not actor.static_mesh_component.get_editor_property('can_ever_affect_navigation')
assert digest(maps['L_Aurelion_M12.umap']) == before['L_Aurelion_M12.umap']
after = digest(maps['L_Aurelion_M13.umap'])
assert after != before['L_Aurelion_M13.umap']
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'canopy-save.json').write_text(json.dumps(dict(
    status='saved_reloaded', reviewed_preview=str(review_dir),
    placement_sha256=placement['digest'](rows), instances=len(rows),
    source_fbx_sha256=review['source_fbx_sha256'],
    m12_sha256=before['L_Aurelion_M12.umap'], m13_sha256_before=before['L_Aurelion_M13.umap'],
    m13_sha256_after=after, m13_backup=str(backup),
    preexisting_actor_state_preserved=True, all_new_collision_disabled=True,
    all_new_nav_disabled=True, qualification='Editor save and reload; PIE recovery and performance remain separate.'
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'M13_ROUTE_VIEWS': [('dominion-through-view', (0, 47500, 180)),
                        ('reformation-through-view', (0, 47500, 180))],
    'M13_ROUTE_YAWS': {'dominion-through-view': 180, 'reformation-through-view': 0},
    'M13_ROUTE_PITCHES': {'dominion-through-view': 5, 'reformation-through-view': 5},
})
print('Z12_CANOPY_SAVED_RELOADED_PASS')
