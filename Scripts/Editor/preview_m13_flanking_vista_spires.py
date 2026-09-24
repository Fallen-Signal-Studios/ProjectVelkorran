"""Unsaved player-eye comparison of custom spires beside the Z12 shuttles."""
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
maps = {name: root / 'Content/Aurelion/Maps' / name
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name: digest(path) for name, path in maps.items()}
assert before['L_Aurelion_M13.umap'] == '29f693af32c51abef71856ad7c87c0ed5e4cb419c9ce6f885e34db9695feaffb'
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
existing = list(api.get_all_level_actors())
baseline = helper['snapshot_actor_state'](existing)
source = root / 'Art/Source/Aurelion/Z12OpenVistaKit'
manifest = json.loads((source / 'manifest.json').read_text())
assert json.loads((source / 'verification.json').read_text())['status'] == 'round_trip_pass'
assetroot = '/Game/Aurelion/Environment/ArchitectureKit/Meshes'
rows = []
for faction, sign in (('Dominion', -1), ('Reformation', 1)):
    for end, y, kind in (('Aft', 46250, 'Tall'), ('Fore', 48750, 'Short')):
        path = assetroot + '/SM_Aurelion_KIT_Z12VistaSpire' + kind
        mesh = unreal.load_asset(path)
        assert isinstance(mesh, unreal.StaticMesh), path
        position = unreal.Vector(sign * 5550, y, 0)
        actor = api.spawn_actor_from_class(unreal.StaticMeshActor, position)
        actor.set_actor_label('PREVIEW_Z12_' + faction + '_FlankingSpire_' + end)
        actor.set_folder_path('Aurelion/Z12/UnsavedVistaStudy')
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_collision_profile_name('NoCollision')
        component.set_editor_property('can_ever_affect_navigation', False)
        actor.set_actor_enable_collision(False)
        assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        rows.append(dict(label=actor.get_actor_label(), mesh=mesh.get_path_name(),
                         location=[position.x, position.y, position.z]))
assert helper['snapshot_actor_state'](existing) == baseline
assert {name: digest(path) for name, path in maps.items()} == before
(out / 'flanking-spire-preview.json').write_text(json.dumps(dict(
    status='unsaved_visual_preview', reference=manifest['design_reference'],
    map_hashes_before=before, existing_actors_preserved=True,
    placements=rows, collision_and_navigation_disabled=True,
    qualification='Unreal fixed eye-height review; do not save without visual acceptance.'
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [('west-flank', (-900, 47200, 180)),
                        ('east-flank', (900, 47800, 180)),
                        ('west-close-flank', (-1500, 47500, 180)),
                        ('east-close-flank', (1500, 47500, 180))],
    'M13_ROUTE_YAWS': {'west-flank': 180, 'east-flank': 0,
                       'west-close-flank': 180, 'east-close-flank': 0},
    'M13_ROUTE_PITCHES': {'west-flank': 8, 'east-flank': 8,
                          'west-close-flank': 8, 'east-close-flank': 8},
})
