"""Guarded faction armor palette correction and unsaved M13 berth review."""
import hashlib
import json
import os
import runpy
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
map_files = {name: root / 'Content/Aurelion/Maps' / name
             for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
map_hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest()
              for name, path in map_files.items()}
assert map_hashes['L_Aurelion_M13.umap'] == 'ce18e4035a2ce6726d2970c1669a3255937ec7327b918aba06aef0187dc27250'
dest = '/Game/Aurelion/Environment/Blender/Shuttles'
rows = []
for faction, old_rgb in (('Dominion', (.22, .032, .024)),
                         ('Reformation', (.032, .065, .105))):
    key = f'M_{faction}_Armor'
    spec = json.loads((root / 'Art/Source/Aurelion' /
                       f'SM_Aurelion_{faction}_Shuttle.json').read_text(encoding='utf8'))
    target = spec['materials'][key]
    path = dest + '/' + key
    material = unreal.load_asset(path)
    assert isinstance(material, unreal.Material)
    refs = set(unreal.EditorAssetLibrary.find_package_referencers_for_asset(path))
    assert refs == {dest + f'/SM_Aurelion_{faction}_Shuttle'}, refs
    color = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_ROUGHNESS)
    assert isinstance(color, unreal.MaterialExpressionConstant3Vector)
    assert isinstance(roughness, unreal.MaterialExpressionConstant)
    original = color.get_editor_property('constant')
    before_rgb = [original.r, original.g, original.b]
    assert all(abs(a-b) < .0001 for a, b in zip(before_rgb, old_rgb)), before_rgb
    before_rough = roughness.get_editor_property('r')
    assert abs(before_rough-.32) < .0001, before_rough
    color.set_editor_property('constant', unreal.LinearColor(*target['rgb'], 1))
    roughness.set_editor_property('r', target['rough'])
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    rows.append(dict(material=path, referencers=sorted(refs), old_rgb=before_rgb,
                     new_rgb=target['rgb'], old_roughness=before_rough,
                     new_roughness=target['rough']))

assert {name: hashlib.sha256(path.read_bytes()).hexdigest()
        for name, path in map_files.items()} == map_hashes
dirty_maps = [package.get_name() for package in
              unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
assert set(dirty_maps).issubset({'/Game/Aurelion/Maps/L_Aurelion_M13'})
(out / 'shuttle-armor-palette.json').write_text(json.dumps(dict(
    status='saved_materials_map_unchanged', changes=rows,
    map_hashes=map_hashes, unsaved_dirty_maps=dirty_maps), indent=2),
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
