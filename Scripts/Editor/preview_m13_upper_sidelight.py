"""Import and inspect four Z12 upper sidelights without saving M13."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
maps = {name: root / 'Content/Aurelion/Maps' / name for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {name: digest(path) for name, path in maps.items()}
assert before['L_Aurelion_M13.umap'] == '3b7e24d4e44f145097fc1424cde5f8c6c43db54477e78f0b961040bdd99c14af'

source = root / 'Art/Source/Aurelion/Z12UpperSidelight'
manifest = json.loads((source / 'manifest.json').read_text())
assert json.loads((source / 'verification.json').read_text())['status'] == 'round_trip_pass'
specs = manifest['modules']
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
assetroot = '/Game/Aurelion/Environment/ArchitectureKit'
materials = {
    'M_Aurelion_ViewBlackStone': assetroot + '/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_IvoryStone': assetroot + '/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_ChannelShadow': '/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim',
    'M_Aurelion_AncientGold': assetroot + '/Materials/M_AurelionKit_Gold',
    'M_Aurelion_DarkSteel': assetroot + '/Materials/M_AurelionKit_PavingBasalt',
    'M_Aurelion_ViewWarmConduit': '/Game/Aurelion/Art/Props/aURELION_pILLAR/Materials/M_GoldEmmissive',
    'M_Aurelion_ViewGlass': assetroot + '/Materials/M_AurelionKit_ViewGlass',
}
assert all(unreal.EditorAssetLibrary.does_asset_exist(path) for path in materials.values())
meshes = {spec['asset']: helper['import_owned_mesh'](spec, source, assetroot + '/Meshes', materials)
          for spec in specs}
frame = meshes['SM_Aurelion_KIT_Z12UpperSidelightFrame_4x3']
pane = meshes['SM_Aurelion_KIT_Z12UpperSidelightPane_4x3']
sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert not sm.get_nanite_settings(pane).get_editor_property('enabled')

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
assert remove == [4, 5, 6, 7, 16, 17, 18, 19], remove
registers = [by_label['PREVIEW_Aurelion_Z12_ServiceRegister_%02d' % i]
             for i in (4, 5, 8, 9, 20, 21, 24, 25)]
assert all(a.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
           for a in registers)
others = helper['snapshot_actor_state']([a for a in existing if a != owner])
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
        for role, mesh in (('Frame', frame), ('Pane', pane)):
            actor = api.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, 450))
            assert actor
            actor.set_actor_label('PREVIEW_Z12_%sUpperSidelight_%s_%s' % (direction, station, role))
            actor.set_actor_rotation(unreal.Rotator(yaw=-90 if x < 0 else 90), True)
            comp = actor.static_mesh_component
            comp.set_static_mesh(mesh)
            comp.set_collision_profile_name('NoCollision')
            comp.set_editor_property('can_ever_affect_navigation', False)
            if role == 'Pane':
                comp.set_cast_shadow(False)
            actor.set_actor_enable_collision(False)
            created.append(actor)
assert len(created) == 8
assert helper['snapshot_actor_state']([a for a in existing if a != owner]) == others
assert {name: digest(path) for name, path in maps.items()} == before
(out / 'upper-sidelight-preview.json').write_text(json.dumps(dict(
    status='unsaved_visual_preview', map_hashes_before=before, map_files_unchanged=True,
    removed_coffer_indices=remove, lower_service_registers_retained=True,
    native_wall_and_rib_collision_retained=True, other_actor_state_retained=True,
    visual_only=True, mesh_paths={name: mesh.get_path_name() for name, mesh in meshes.items()},
    mesh_source_sha256={s['asset']: digest(source / (s['asset'] + '.fbx')) for s in specs},
    placed_labels=[a.get_actor_label() for a in created]), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [('west-upper-sidelight', (-900, 47200, 180)),
                        ('east-upper-sidelight', (900, 47800, 180))],
    'M13_ROUTE_YAWS': {'west-upper-sidelight': 180, 'east-upper-sidelight': 0},
    'M13_ROUTE_PITCHES': {'west-upper-sidelight': 8, 'east-upper-sidelight': 8},
})
print('M13_UPPER_SIDELIGHT_UNSAVED_READY')
