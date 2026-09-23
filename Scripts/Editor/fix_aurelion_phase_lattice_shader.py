"""Repair the existing Elite phase lattice's reserved HLSL identifier."""
import hashlib
import json
import os
import shutil
from pathlib import Path

import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

path = '/Game/Aurelion/Enemies/Materials/M_AurelionPhaseLattice'
material = unreal.load_asset(path)
assert isinstance(material, unreal.Material)
shapes = []
for expression in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
    outer = expression.get_outer()
    while outer:
        if outer == material:
            shapes.append(expression)
            break
        outer = outer.get_outer()
assert len(shapes) == 1
shape = shapes[0]
before = shape.get_editor_property('code')
old_declaration = 'float line = 1.0 - smoothstep(0.025, 0.065, bandDistance);'
old_use = 'float lattice = saturate(line + rung * 0.32);'
assert before.count(old_declaration) == 1 and before.count(old_use) == 1
after = before.replace(old_declaration, old_declaration.replace('line', 'bandLine', 1))
after = after.replace(old_use, old_use.replace('(line ', '(bandLine ', 1))
assert after != before and 'float line =' not in after

disk = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/Enemies/Materials/M_AurelionPhaseLattice.uasset'
assert disk.is_file()
backup = out / (disk.name + '.before')
shutil.copy2(disk, backup)
assert hashlib.sha256(disk.read_bytes()).digest() == hashlib.sha256(backup.read_bytes()).digest()
shape.set_editor_property('code', after)
unreal.MaterialEditingLibrary.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'phase-lattice-shader-fix.json').write_text(json.dumps(dict(
    status='saved_requires_pie_shader_compile', material=path,
    source_before=before, source_after=after,
    asset_sha256_before=hashlib.sha256(backup.read_bytes()).hexdigest(),
    asset_sha256_after=hashlib.sha256(disk.read_bytes()).hexdigest(),
    backup=str(backup)), indent=2))
