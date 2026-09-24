"""Unsaved two-light trial to expose each Z12 shuttle through the concourse glass."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
existing = list(actors.get_all_level_actors())
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
baseline = helper['snapshot_actor_state'](existing)
maps = {name: root / 'Content/Aurelion/Maps' / name
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
before = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in maps.items()}

specs = (
    ('Dominion', (-2550, 47500, 600), 180, (1.0, .56, .34, 1.0)),
    ('Reformation', (2550, 47500, 600), 0, (.58, .84, 1.0, 1.0)),
)
lights = []
for name, position, yaw, color in specs:
    actor = actors.spawn_actor_from_class(unreal.RectLight, unreal.Vector(*position))
    actor.set_actor_label('PREVIEW_Z12_BerthViewLight_' + name)
    actor.set_actor_rotation(unreal.Rotator(pitch=-12, yaw=yaw), True)
    component = actor.get_component_by_class(unreal.RectLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
    component.set_intensity(6000)
    component.set_attenuation_radius(1900)
    component.set_source_width(850)
    component.set_source_height(400)
    component.set_light_color(unreal.LinearColor(*color))
    component.set_editor_property('specular_scale', .45)
    component.set_editor_property('cast_shadows', True)
    lights.append(actor)
assert helper['snapshot_actor_state'](existing) == baseline
assert {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in maps.items()} == before
(out / 'berth-light-preview.json').write_text(json.dumps(dict(
    status='unsaved_preview', maps_before=before,
    specs=[dict(name=n, position=p, yaw=y, color=c) for n, p, y, c in specs],
    intensity_lumens=6000, attenuation_radius_cm=1900, source_width_cm=850,
    source_height_cm=400, pitch_degrees=-12, preexisting_actor_state_preserved=True,
    limitation='Editor-only visual preview; gameplay, cost and final color require later checks.'
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [('dominion-lit', (0, 47500, 180)),
                        ('reformation-lit', (0, 47500, 180))],
    'M13_ROUTE_YAWS': {'dominion-lit': 180, 'reformation-lit': 0},
    'M13_ROUTE_PITCHES': {'dominion-lit': 5, 'reformation-lit': 5},
})
print('Z12_BERTH_LIGHTS_UNSAVED_PREVIEW_PASS')
