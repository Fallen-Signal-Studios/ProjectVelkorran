"""Import the custom Z12 berth wall and judge both docks without saving M13."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion/Z12BerthBulkhead'
manifest = json.loads((source / 'manifest.json').read_text())
assert json.loads((source / 'verification.json').read_text())['status'] == 'round_trip_pass'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
existing = list(actors.get_all_level_actors())
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
original = helper['snapshot_actor_state'](existing)
maps = {name: root / 'Content/Aurelion/Maps' / name
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
before = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in maps.items()}

dest = '/Game/Aurelion/Environment/ArchitectureKit'
materials = {key: dest + '/Materials/M_AurelionKit_' + name for key, name in {
    'M_Aurelion_IvoryStone': 'PavingIvory',
    'M_Aurelion_AncientGold': 'Gold',
    'M_Aurelion_ChannelShadow': 'Reveal',
    'M_Aurelion_DarkSteel': 'PavingBasalt',
    'M_Aurelion_BlackStone': 'ObservationBlackStone',
    'M_Aurelion_LumenLens': 'UplightLens',
}.items()}
meshes = {spec['asset']: helper['import_owned_mesh'](spec, source, dest + '/Meshes', materials)
          for spec in manifest['modules']}

rows = []
for faction, cx, yaw in (('Dominion', -3800, 90), ('Reformation', 3800, -90)):
    rear_x = cx + (-1320 if cx < 0 else 1320)
    for index, y_offset in enumerate((-622.5, -207.5, 207.5, 622.5)):
        kind = 'Service' if index in (1, 2) else 'Plain'
        asset = 'SM_Aurelion_KIT_Z12BerthBulkhead' + kind
        row = dict(label='Aurelion_Z12_%s_BerthBulkhead_%d' % (faction, index),
                   asset=asset, location=[rear_x, 47500 + y_offset, 0], yaw=yaw)
        rows.append(row)
    # Side returns stop short of the protected concourse view. They close the
    # sky behind each scenic shuttle while leaving the original glass/boundary.
    for side, y in (('South', 46670), ('North', 48330)):
        side_yaw = 180 if side == 'South' else 0
        for index in range(6):
            x = cx + (-1112.5 + index * 415) * (-1 if cx > 0 else 1)
            kind = 'Service' if index in (2, 3) else 'Plain'
            rows.append(dict(label='Aurelion_Z12_%s_BerthReturn_%s_%d' % (faction, side, index),
                             asset='SM_Aurelion_KIT_Z12BerthBulkhead' + kind,
                             location=[x, y, 0], yaw=side_yaw))

created = []
for row in rows:
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*row['location']))
    actor.set_actor_label('PREVIEW_' + row['label'])
    actor.set_actor_rotation(unreal.Rotator(yaw=row['yaw']), False)
    component = actor.static_mesh_component
    component.set_static_mesh(meshes[row['asset']])
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    created.append(actor)

light_specs = []
for faction, x, yaw, color in (
    ('Dominion', -4350, 180, (1.0, .64, .43, 1.0)),
    ('Reformation', 4350, 0, (.66, .84, 1.0, 1.0)),
):
    for index, y in enumerate((47150, 47850)):
        light = actors.spawn_actor_from_class(unreal.RectLight, unreal.Vector(x, y, 380))
        light.set_actor_label('PREVIEW_Z12_%s_BerthBulkheadWash_%d' % (faction, index))
        light.set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw), True)
        component = light.get_component_by_class(unreal.RectLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
        component.set_intensity(1800)
        component.set_attenuation_radius(1450)
        component.set_source_width(550)
        component.set_source_height(390)
        component.set_light_color(unreal.LinearColor(*color))
        component.set_editor_property('specular_scale', .35)
        component.set_editor_property('cast_shadows', False)
        light_specs.append(dict(faction=faction, index=index, location=[x, y, 380],
                                yaw=yaw, color=color, lumens=1800, attenuation_cm=1450,
                                source_width_cm=550, source_height_cm=390, cast_shadows=False))

assert helper['snapshot_actor_state'](existing) == original
assert all(a.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           for a in created)
assert {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in maps.items()} == before
(out / 'berth-bulkhead-preview.json').write_text(json.dumps(dict(
    status='unsaved_preview', rows=rows, mesh_paths={k: v.get_path_name() for k, v in meshes.items()},
    source_fbx_sha256={spec['asset']: hashlib.sha256((source / (spec['asset'] + '.fbx')).read_bytes()).hexdigest()
                       for spec in manifest['modules']},
    map_hashes_before=before, preexisting_actor_state_preserved=True,
    all_preview_collision_disabled=True, light_specs=light_specs,
    design_reference=manifest['design_reference'],
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [('dominion-through-view', (0, 47500, 180)),
                        ('reformation-through-view', (0, 47500, 180)),
                        ('dominion-rear-oblique', (-3800, 46800, 240)),
                        ('reformation-rear-oblique', (3800, 46800, 240))],
    'M13_ROUTE_YAWS': {'dominion-through-view': 180, 'reformation-through-view': 0,
                       'dominion-rear-oblique': 145, 'reformation-rear-oblique': 35},
    'M13_ROUTE_PITCHES': {'dominion-through-view': 5, 'reformation-through-view': 5,
                          'dominion-rear-oblique': 5, 'reformation-rear-oblique': 5},
})
