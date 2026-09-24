"""Save only the reviewed red/white Z12 scenic plates; leave spires unplaced."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
west_review = root / 'Saved/Validation/Aurelion/Z12OpenVistaWindowReview-20260923-214515-a3e16c1d/open-vista-preview.json'
east_review = root / 'Saved/Validation/Aurelion/Z12NearVistaSpireReview-20260923-215754-6f316827/open-vista-preview.json'
west = json.loads(west_review.read_text())
east = json.loads(east_review.read_text())
assert west['status'] == east['status'] == 'unsaved_preview'
assert west['maps_before'] == east['maps_before']
assert west['preexisting_actor_state_preserved'] and east['preexisting_actor_state_preserved']
assert west['no_preview_collision'] and east['no_preview_collision']
root_maps = {name: root / 'Content/Aurelion/Maps' / name
             for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name: digest(path) for name, path in root_maps.items()}
assert before == west['maps_before']
assert before['L_Aurelion_M13.umap'] == 'ce18e4035a2ce6726d2970c1669a3255937ec7327b918aba06aef0187dc27250'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
existing = list(actors.get_all_level_actors())
baseline = helper['snapshot_actor_state'](existing)

specs = (
    ('Dominion', -8700, 400, 180, 'return float2(UV.y * 0.42, 1.0-UV.x);', west),
    ('Reformation', 8700, 0, 0,
     'return float2(0.85 + UV.y * 0.14, 0.25 + (1.0-UV.x) * 0.45);', east),
)
created = []
for faction, x, z, yaw, code, preview in specs:
    label = 'Aurelion_Z12_%s_RemnantPlate' % faction
    assert not any(a.get_actor_label() == label for a in existing)
    material_path = '/Game/Aurelion/Environment/ObservationVista/Materials/M_Aurelion_Z12%sRemnant' % faction
    material = unreal.load_asset(material_path)
    assert isinstance(material, unreal.Material)
    edit = unreal.MaterialEditingLibrary
    sample = edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    custom = [node for node in edit.get_inputs_for_material_expression(material, sample)
              if isinstance(node, unreal.MaterialExpressionCustom)]
    assert len(custom) == 1 and custom[0].get_editor_property('code') == code
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                         unreal.Vector(x, 47500, z),
                                         unreal.Rotator(pitch=90, yaw=yaw))
    actor.set_actor_label(label)
    actor.set_folder_path('Aurelion/Z12/RemnantVista')
    actor.set_actor_scale3d(unreal.Vector(30, 40, 1))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    component.set_material(0, material)
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    component.set_cast_shadow(False)
    component.set_editor_property('receives_decals', False)
    actor.set_actor_enable_collision(False)
    preview_row = next(p for p in preview['plates'] if p['label'].endswith(label))
    assert actor.get_actor_transform().export_text() == preview_row['transform']
    assert component.get_material(0).get_path_name() == preview_row['material']
    created.append(actor)

assert len(created) == 2 and helper['snapshot_actor_state'](existing) == baseline
assert all(a.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           and not a.static_mesh_component.get_editor_property('can_ever_affect_navigation')
           and not a.get_actor_enable_collision() for a in created)
labels = {a.get_actor_label() for a in created}
assert {name: digest(path) for name, path in root_maps.items()} == before
backup = out / 'L_Aurelion_M13.before.umap'
shutil.copy2(root_maps['L_Aurelion_M13.umap'], backup)
assert level.save_current_level()
assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
reloaded = list(actors.get_all_level_actors())
assert helper['snapshot_actor_state']([a for a in reloaded if a.get_actor_label() not in labels]) == baseline
for faction, x, z, yaw, code, preview in specs:
    label = 'Aurelion_Z12_%s_RemnantPlate' % faction
    found = [a for a in reloaded if a.get_actor_label() == label]
    assert len(found) == 1
    actor = found[0]
    component = actor.static_mesh_component
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not component.get_editor_property('can_ever_affect_navigation')
    assert not actor.get_actor_enable_collision()
    assert actor.get_actor_transform().export_text() == next(p for p in preview['plates']
                                                              if p['label'].endswith(label))['transform']
assert not any(a.get_actor_label().startswith('Aurelion_Z12_')
               and 'OpenVista_' in a.get_actor_label() for a in reloaded)
after = {name: digest(path) for name, path in root_maps.items()}
assert after['L_Aurelion_M12.umap'] == before['L_Aurelion_M12.umap']
assert after['L_Aurelion_M13.umap'] != before['L_Aurelion_M13.umap']
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'remnant-plate-save.json').write_text(json.dumps(dict(
    status='saved_reloaded', west_preview=str(west_review), east_preview=str(east_review),
    m13_backup=str(backup), maps_before=before, maps_after=after,
    preexisting_actor_state_preserved=True, saved_plate_count=2,
    no_spire_placement=True, collision_disabled=True, navigation_disabled=True,
    qualification='Editor save/reload and fixed-camera views; live CP9 and performance separate.'
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'M13_ROUTE_VIEWS': [('dominion-window', (-900, 47200, 180)),
                        ('reformation-window', (900, 47800, 180))],
    'M13_ROUTE_YAWS': {'dominion-window': 180, 'reformation-window': 0},
    'M13_ROUTE_PITCHES': {'dominion-window': 9, 'reformation-window': 9},
})
print('Z12_REMNANT_PLATES_SAVED_RELOADED_PASS')
