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
placement = runpy.run_path(str(root / 'Scripts/Editor/aurelion_z12_canopy_plan.py'))
planned = placement['plan'](manifest)
meshes = {}
for spec in manifest['modules']:
    assert spec['uv_layers'] == 2 and spec['convex_hulls'] == 0
    mesh = helper['import_owned_mesh'](spec, source, dest + '/Meshes', materials)
    meshes[spec['asset']] = mesh

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


for row in planned:
    visual('PREVIEW_' + row['label'], meshes[row['asset']], row['location'])

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
    placement_sha256=placement['digest'](planned),
    source_fbx_sha256={spec['asset']: hashlib.sha256((source / (spec['asset'] + '.fbx')).read_bytes()).hexdigest()
                       for spec in manifest['modules']},
    map_hashes_before=maps, map_actor_state_preserved=True, no_preview_collision=True,
    design_reference=manifest['design_reference'],
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [
        ('dominion-arrival', (-3800, 46250, 200)),
        ('dominion-from-lounge', (-1250, 47600, 185)),
        ('reformation-arrival', (3800, 46250, 200)),
        ('dominion-through-view', (0, 47500, 180)),
        ('reformation-through-view', (0, 47500, 180)),
    ],
    'M13_ROUTE_YAWS': {'dominion-arrival': 90, 'dominion-from-lounge': 180, 'reformation-arrival': 90,
                       'dominion-through-view': 180, 'reformation-through-view': 0},
    'M13_ROUTE_PITCHES': {'dominion-arrival': 8, 'dominion-from-lounge': 7, 'reformation-arrival': 8,
                          'dominion-through-view': 5, 'reformation-through-view': 5},
})
