"""Import authored Z12 canopy modules and preview them without saving M13."""
from pathlib import Path
import hashlib
import json
import os
import runpy

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion/Z12DepartureCanopy'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
existing = list(actors.get_all_level_actors())
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
original = helper['snapshot_actor_state'](existing)
maps = {name: hashlib.sha256((root / 'Content/Aurelion/Maps' / name).read_bytes()).hexdigest()
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}

dest = '/Game/Aurelion/Environment/ArchitectureKit'
materials = {key: dest + '/Materials/M_AurelionKit_' + name for key, name in {
    'M_Aurelion_IvoryStone': 'PavingIvory',
    'M_Aurelion_AncientGold': 'Gold',
    'M_Aurelion_ChannelShadow': 'Reveal',
    'M_Aurelion_DarkSteel': 'PavingBasalt',
    'M_Aurelion_BlackStone': 'ObservationBlackStone',
    'M_Aurelion_LumenLens': 'UplightLens',
}.items()}
manifest = json.loads((source / 'manifest.json').read_text())
meshes = {}
for spec in manifest['modules']:
    assert spec['uv_layers'] == 2 and spec['convex_hulls'] == 0
    mesh = helper['import_owned_mesh'](spec, source, dest + '/Meshes', materials)
    meshes[spec['asset']] = mesh

assembly = manifest['dock_assembly']
assert assembly['berth_m'] == [28, 18]
assert assembly['center_clear_lane_m'] >= 5
created = []


def visual(name, mesh, position):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position))
    actor.set_actor_label(name)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property('can_ever_affect_navigation', False)
    created.append(actor)
    return actor


def height(x):
    return 7.5 + 1.2 * (1.0 - (x / 13.2) ** 2)


for dock, cx in (('Dominion', -3800), ('Reformation', 3800)):
    for index, y in enumerate(assembly['rib_stations_y_m']):
        name = 'SM_Aurelion_KIT_Z12CanopyTerminalRib' if index == 0 else 'SM_Aurelion_KIT_Z12CanopyVaultRib'
        visual('PREVIEW_Z12_%s_Rib_%d' % (dock, index), meshes[name],
               (cx, 47500 + y * 100, 0))
        for side, x in enumerate(assembly['foot_x_m']):
            visual('PREVIEW_Z12_%s_Foot_%d_%d' % (dock, index, side),
                   meshes['SM_Aurelion_KIT_Z12CanopyBearingFoot'],
                   (cx + x * 100, 47500 + y * 100, 0))
    for i, x in enumerate(assembly['coffer_x_m']):
        for j, y in enumerate(assembly['coffer_y_m']):
            visual('PREVIEW_Z12_%s_Coffer_%d_%d' % (dock, i, j),
                   meshes['SM_Aurelion_KIT_Z12CanopyCoffer_4x4'],
                   (cx + x * 100, 47500 + y * 100, (height(x) + .13) * 100))
    for i, x in enumerate(assembly['pendant_x_m']):
        for j, y in enumerate(assembly['pendant_y_m']):
            visual('PREVIEW_Z12_%s_Pendant_%d_%d' % (dock, i, j),
                   meshes['SM_Aurelion_KIT_Z12CanopyPendant'],
                   (cx + x * 100, 47500 + y * 100, (height(x) - .1) * 100))

assert helper['snapshot_actor_state'](existing) == original
assert all(p.get_name() == '/Game/Aurelion/Maps/L_Aurelion_M13'
           for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
for actor in created:
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
collisions = [(a.get_actor_label(), str(a.static_mesh_component.get_collision_enabled())) for a in created]
(out / 'canopy-collision-diagnostic.json').write_text(json.dumps(collisions, indent=2))
assert all(a.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION for a in created)

(out / 'canopy-preview.json').write_text(json.dumps(dict(
    status='unsaved_preview', instances=len(created), mesh_paths={k: v.get_path_name() for k, v in meshes.items()},
    map_hashes_before=maps, map_actor_state_preserved=True, no_preview_collision=True,
    design_reference=manifest['design_reference'],
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [
        ('dominion-arrival', (-3800, 46250, 200)),
        ('dominion-from-lounge', (-1250, 47600, 185)),
        ('reformation-arrival', (3800, 46250, 200)),
    ],
    'M13_ROUTE_YAWS': {'dominion-arrival': 90, 'dominion-from-lounge': 180, 'reformation-arrival': 90},
    'M13_ROUTE_PITCHES': {'dominion-arrival': 8, 'dominion-from-lounge': 7, 'reformation-arrival': 8},
})
