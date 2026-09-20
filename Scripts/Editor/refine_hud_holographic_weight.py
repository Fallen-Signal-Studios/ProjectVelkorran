"""Thin the reference footer and reduce opaque HUD backing; preserve live inputs."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
edit = unreal.MaterialEditingLibrary
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
patches = {
    'M_SovPlateHousing': {
        'glass*.97+glyph+outerHalo*.62+capHalo*.30':
        'glass*.62+rim*.35+capRim*.35+shelfRim*.35+shieldRim*.35+glyph+outerHalo*.28+capHalo*.18',
        'outerHalo*.35+capHalo*.14': 'outerHalo*.18+capHalo*.10',
    },
    'M_SovEchoSegmentedArc': {
        'float h=min(.145,': 'float h=min(.112,',
        '.086': '.059',
        'curve+.057': 'curve+.037',
        'curve-.19': 'curve-.145',
        'body*.97+chev+fine*.55+halo*.40':
        'body*.38+rim*.50+inset*cell*ready*.48+chev+fine*.55+halo*.20',
        'halo*.18;': 'halo*.09;',
    },
}
pending = []
for name, replacements in patches.items():
    material = unreal.load_asset('/Game/Aurelion/UI/HUD/' + name)
    visited, shapes = set(), []
    def visit(node):
        if not node or node.get_path_name() in visited:
            return
        visited.add(node.get_path_name())
        if isinstance(node, unreal.MaterialExpressionCustom):
            shapes.append(node)
        for upstream in edit.get_inputs_for_material_expression(material, node):
            visit(upstream)
    visit(edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    assert len(shapes) == 1, name
    shape = shapes[0]
    before = shape.get_editor_property('code')
    after = before
    for source, replacement in replacements.items():
        assert source in after, 'Unexpected saved shader revision: ' + name + ': ' + source
        after = after.replace(source, replacement)
    pending.append((name, material, shape, before, after))

report = dict(status='saved_requires_rendered_review', materials=[], gameplay_bindings_changed=False)
for name, material, shape, before, after in pending:
    disk = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/UI/HUD' / (name + '.uasset')
    backup = out / (name + '.before.uasset')
    assert not backup.exists()
    shutil.copy2(disk, backup)
    (out / (name + '.before.hlsl')).write_text(before)
    shape.set_editor_property('code', after)
    edit.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    (out / (name + '.after.hlsl')).write_text(after)
    report['materials'].append(material.get_path_name())
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'holographic-weight.json').write_text(json.dumps(report, indent=2))
