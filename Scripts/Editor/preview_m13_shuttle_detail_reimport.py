"""Reimport the owned shuttle FBXs and capture two M13 berth views; never save the map."""
import hashlib
import json
import os
import runpy
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion'
map_files = {name: root / 'Content/Aurelion/Maps' / name
             for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
map_hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest()
              for name, path in map_files.items()}
assert map_hashes['L_Aurelion_M13.umap'] == 'ce18e4035a2ce6726d2970c1669a3255937ec7327b918aba06aef0187dc27250'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
before = helper['snapshot_actor_state'](actors.get_all_level_actors())
destination = '/Game/Aurelion/Environment/Blender/Shuttles'
rows = []

for faction in ('Dominion', 'Reformation'):
    name = f'SM_Aurelion_{faction}_Shuttle'
    metadata = json.loads((source / f'{name}.json').read_text(encoding='utf8'))
    fbx = source / f'{name}.fbx'
    assert hashlib.sha256(fbx.read_bytes()).hexdigest() == metadata['fbx_sha256']
    path = destination + '/' + name
    old = unreal.load_asset(path)
    assert isinstance(old, unreal.StaticMesh)
    old_materials = {
        str(slot.get_editor_property('imported_material_slot_name')):
        slot.get_editor_property('material_interface').get_path_name()
        for slot in old.get_editor_property('static_materials')}
    assert set(old_materials) == set(metadata['materials'])
    material_graphs = {}
    for key, material_path in old_materials.items():
        material = unreal.load_asset(material_path)
        try:
            material_graphs[key] = [node.get_class().get_name() for node in
                                    material.get_editor_property('expressions')]
        except Exception:
            material_graphs[key] = ['unavailable']
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    old_nanite = bool(sm.get_nanite_settings(old).get_editor_property('enabled'))
    spec = dict(asset=name, materials=list(metadata['materials']),
                nominal_dimensions_m=metadata['dimensions_metres'],
                nanite_enabled=old_nanite, convex_hulls=0)
    mesh = helper['import_owned_mesh'](spec, source, destination, old_materials)
    assert mesh.get_path_name() == old.get_path_name()
    assert sm.get_num_uv_channels(mesh, 0) == 2
    rows.append(dict(asset=path, fbx_sha256=metadata['fbx_sha256'],
                     triangles=metadata['triangles'],
                     dimensions_metres=metadata['dimensions_metres'],
                     materials=old_materials, material_graphs=material_graphs,
                     nanite_enabled=old_nanite,
                     uv_channels=2, collision='none'))

assert helper['snapshot_actor_state'](actors.get_all_level_actors()) == before
dirty_maps = [package.get_name() for package in
              unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
# Reimport invalidates the loaded actor's render state and marks M13 dirty in
# the editor. It is deliberately never saved by this asset-only preview.
assert set(dirty_maps).issubset({'/Game/Aurelion/Maps/L_Aurelion_M13'})
assert {name: hashlib.sha256(path.read_bytes()).hexdigest()
        for name, path in map_files.items()} == map_hashes
(out / 'shuttle-detail-reimport.json').write_text(json.dumps(dict(
    status='assets_reimported_map_unchanged',
    source_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Departure-Shuttles-Detail.png',
    map_hashes=map_hashes, actor_state_unchanged=True,
    unsaved_dirty_maps=dirty_maps, shuttles=rows), indent=2),
    encoding='utf8')

runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW': True,
    'M13_ROUTE_VIEWS': [
        ('dominion-close', (-1500, 47500, 180)),
        ('reformation-close', (1500, 47500, 180)),
        ('dominion-dock', (-3800, 46400, 180)),
        ('reformation-dock', (3800, 46400, 180)),
    ],
    'M13_ROUTE_YAWS': {'dominion-close': 180, 'reformation-close': 0,
                        'dominion-dock': 90, 'reformation-dock': 90},
    'M13_ROUTE_PITCHES': {'dominion-close': 5, 'reformation-close': 5,
                           'dominion-dock': 6, 'reformation-dock': 6},
})
