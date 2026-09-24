"""Import and temporarily compose open Z12 scenic silhouettes and remnant plates."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion/Z12OpenVistaKit'
manifest = json.loads((source / 'manifest.json').read_text())
assert json.loads((source / 'verification.json').read_text())['status'] == 'round_trip_pass'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
existing = list(actors.get_all_level_actors())
baseline = helper['snapshot_actor_state'](existing)
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

base_path = '/Game/Aurelion/Environment/ObservationVista/Materials/M_Aurelion_Z11WoundVista'
plate_materials = {}
for faction, code in (
    ('Dominion', 'return float2(UV.y * 0.42, 1.0-UV.x);'),
    ('Reformation', 'return float2(0.85 + UV.y * 0.14, 0.25 + (1.0-UV.x) * 0.45);'),
):
    path = '/Game/Aurelion/Environment/ObservationVista/Materials/M_Aurelion_Z12%sRemnant' % faction
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        assert unreal.EditorAssetLibrary.duplicate_asset(base_path, path)
    material = unreal.load_asset(path)
    assert isinstance(material, unreal.Material)
    edit = unreal.MaterialEditingLibrary
    sample = edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    custom = [node for node in edit.get_inputs_for_material_expression(material, sample)
              if isinstance(node, unreal.MaterialExpressionCustom)]
    assert len(custom) == 1
    custom[0].set_editor_property('code', code)
    edit.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    plate_materials[faction] = material

rows = []
for faction, direction in (('Dominion', -1), ('Reformation', 1)):
    cx = direction * 5550
    for kind, y in (('SpireTall', 46850), ('SpireShort', 48150)):
        rows.append(dict(label='Aurelion_Z12_%s_OpenVista_%s' % (faction, kind),
                         asset='SM_Aurelion_KIT_Z12Vista' + kind,
                         location=[cx, y, 0], yaw=0))
    rows.append(dict(label='Aurelion_Z12_%s_OpenVista_FlyingBridge' % faction,
                     asset='SM_Aurelion_KIT_Z12VistaFlyingBridge',
                     location=[cx, 47500, 1400], yaw=90))

created = []
for row in rows:
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*row['location']))
    actor.set_actor_label('PREVIEW_' + row['label'])
    actor.set_actor_rotation(unreal.Rotator(yaw=row['yaw']), False)
    component = actor.static_mesh_component
    component.set_static_mesh(meshes[row['asset']])
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    actor.set_actor_enable_collision(False)
    created.append(actor)

plates = []
for faction, direction in (('Dominion', -1), ('Reformation', 1)):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                         unreal.Vector(direction * 8700, 47500,
                                                       400 if direction < 0 else 0),
                                         unreal.Rotator(pitch=90, yaw=180 if direction < 0 else 0))
    actor.set_actor_label('PREVIEW_Aurelion_Z12_%s_RemnantPlate' % faction)
    actor.set_actor_scale3d(unreal.Vector(30, 40, 1))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    component.set_material(0, plate_materials[faction])
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    component.set_cast_shadow(False)
    component.set_editor_property('receives_decals', False)
    actor.set_actor_enable_collision(False)
    plates.append(actor)

assert helper['snapshot_actor_state'](existing) == baseline
assert all(a.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           for a in created + plates)
assert {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in maps.items()} == before
(out / 'open-vista-preview.json').write_text(json.dumps(dict(
    status='unsaved_preview', mesh_rows=rows,
    plates=[dict(label=a.get_actor_label(), transform=a.get_actor_transform().export_text(),
                 material=a.static_mesh_component.get_material(0).get_path_name()) for a in plates],
    mesh_paths={k: v.get_path_name() for k, v in meshes.items()},
    source_fbx_sha256={spec['asset']: hashlib.sha256((source / (spec['asset'] + '.fbx')).read_bytes()).hexdigest()
                       for spec in manifest['modules']},
    maps_before=before, preexisting_actor_state_preserved=True, no_preview_collision=True,
    reference=manifest['design_reference'],
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [('reformation-concourse', (0, 47500, 180)),
                        ('reformation-window', (900, 47800, 180))],
    'M13_ROUTE_YAWS': {'reformation-concourse': 0, 'reformation-window': 0},
    'M13_ROUTE_PITCHES': {'reformation-concourse': 7, 'reformation-window': 9},
})
