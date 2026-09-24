"""Save only the two reviewed Z12 berth-view lights, then verify reload."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
review_dir = root / 'Saved/Validation/Aurelion/Z12BerthLightPreview-20260923-190414-acd08550'
review = json.loads((review_dir / 'berth-light-preview.json').read_text())
assert review['status'] == 'unsaved_preview' and review['preexisting_actor_state_preserved']
assert len(review['specs']) == 2
assert all((review_dir / (name + '-lit.png')).is_file() for name in ('dominion', 'reformation'))
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
existing = list(actors.get_all_level_actors())
baseline = helper['snapshot_actor_state'](existing)
maps = {name: root / 'Content/Aurelion/Maps' / name
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name: digest(path) for name, path in maps.items()}
assert before == review['maps_before']
labels = ['Aurelion_Z12_BerthViewLight_' + spec['name'] for spec in review['specs']]
assert not any(a.get_actor_label() in labels for a in existing)


def describe(actor):
    component = actor.get_component_by_class(unreal.RectLightComponent)
    assert component
    return dict(label=actor.get_actor_label(), transform=actor.get_actor_transform().export_text(),
                intensity=component.intensity, radius=component.attenuation_radius,
                width=component.source_width, height=component.source_height,
                color=component.get_light_color().export_text(),
                specular=component.get_editor_property('specular_scale'),
                shadows=component.get_editor_property('cast_shadows'))


created = []
for spec in review['specs']:
    actor = actors.spawn_actor_from_class(unreal.RectLight, unreal.Vector(*spec['position']))
    assert actor
    actor.set_actor_label('Aurelion_Z12_BerthViewLight_' + spec['name'])
    actor.set_folder_path('Aurelion/00_Lighting')
    actor.set_actor_rotation(unreal.Rotator(pitch=review['pitch_degrees'], yaw=spec['yaw']), True)
    component = actor.get_component_by_class(unreal.RectLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
    component.set_intensity(review['intensity_lumens'])
    component.set_attenuation_radius(review['attenuation_radius_cm'])
    component.set_source_width(review['source_width_cm'])
    component.set_source_height(review['source_height_cm'])
    component.set_light_color(unreal.LinearColor(*spec['color']))
    component.set_editor_property('specular_scale', .45)
    component.set_editor_property('cast_shadows', True)
    created.append(actor)
assert helper['snapshot_actor_state'](existing) == baseline
expected = sorted([describe(a) for a in created], key=lambda r: r['label'])
assert {name: digest(path) for name, path in maps.items()} == before
backup = out / 'L_Aurelion_M13.before.umap'
shutil.copy2(maps['L_Aurelion_M13.umap'], backup)
assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
reloaded = list(actors.get_all_level_actors())
assert all(sum(a.get_actor_label() == label for a in reloaded) == 1 for label in labels)
saved = [a for a in reloaded if a.get_actor_label() in labels]
assert sorted([describe(a) for a in saved], key=lambda r: r['label']) == expected
assert helper['snapshot_actor_state']([a for a in reloaded if a not in saved]) == baseline
assert digest(maps['L_Aurelion_M12.umap']) == before['L_Aurelion_M12.umap']
after = digest(maps['L_Aurelion_M13.umap'])
assert after != before['L_Aurelion_M13.umap']
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'berth-light-save.json').write_text(json.dumps(dict(
    status='saved_reloaded', preview=str(review_dir), lights=expected,
    preexisting_actor_state_preserved=True, m12_sha256=before['L_Aurelion_M12.umap'],
    m13_sha256_before=before['L_Aurelion_M13.umap'], m13_sha256_after=after,
    m13_backup=str(backup), qualification='Editor save/reload; PIE and performance separate.'
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'M13_ROUTE_VIEWS': [('dominion-lit', (0, 47500, 180)), ('reformation-lit', (0, 47500, 180))],
    'M13_ROUTE_YAWS': {'dominion-lit': 180, 'reformation-lit': 0},
    'M13_ROUTE_PITCHES': {'dominion-lit': 5, 'reformation-lit': 5},
})
print('Z12_BERTH_LIGHTS_SAVED_RELOADED_PASS')
