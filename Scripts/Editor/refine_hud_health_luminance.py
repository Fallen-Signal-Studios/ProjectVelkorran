"""Brighten the owned health fill's luminous core; preserve native tint and mask."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
edit = unreal.MaterialEditingLibrary
material = unreal.load_asset('/Game/Aurelion/UI/HUD/M_SovHealthBeveledFill')
seen, shapes = set(), []

def visit(node):
    if not node or node.get_path_name() in seen:
        return
    seen.add(node.get_path_name())
    if isinstance(node, unreal.MaterialExpressionCustom):
        shapes.append(node)
    for upstream in edit.get_inputs_for_material_expression(material, node):
        visit(upstream)

visit(edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR))
assert len(shapes) == 1
shape = shapes[0]
before = shape.get_editor_property('code')
after = before
for old, new in (
    ('float energy = lerp(.18,.78,smoothstep(0,1,p.x));',
     'float energy = lerp(.30,.98,smoothstep(0,1,p.x));'),
    ('float depth = lerp(.60,1.0,1-abs(p.y-.5)*2);',
     'float depth = lerp(.72,1.0,1-abs(p.y-.5)*2);'),
    ('float brightness = saturate(energy*depth+upper+lower+rim*.48);',
     'float core = exp(-abs(p.y-.46)*12)*.09;\nfloat brightness = saturate(energy*depth+upper+lower+core+rim*.65);'),
):
    assert before.count(old) == 1, 'Unexpected health material revision: ' + old
    after = after.replace(old, new)
disk = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/UI/HUD/M_SovHealthBeveledFill.uasset'
shutil.copy2(disk, out / 'M_SovHealthBeveledFill.before.uasset')
(out / 'health.before.hlsl').write_text(before, encoding='utf-8')
shape.set_editor_property('code', after)
edit.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
(out / 'health.after.hlsl').write_text(after, encoding='utf-8')
(out / 'health-luminance.json').write_text(json.dumps({
    'status': 'saved_requires_visual_review', 'asset': material.get_path_name(),
    'mask_or_opacity_changed': False, 'widget_or_gameplay_bindings_changed': False,
    'maps_saved': [],
}, indent=2), encoding='utf-8')
