"""Give radar the same translucent glass treatment as the reference HUD footer."""
import json
import os
import shutil
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
material = unreal.load_asset('/Game/Aurelion/UI/HUD/M_SovRadarReticle')
edit = unreal.MaterialEditingLibrary
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
assert before.count('float glass=body*.96;') == 1, 'Unexpected radar shader revision'
assert before.count('glass+rim*.65+arrow+halo*.6') == 1
after = before.replace('float glass=body*.96;', 'float glass=body*.62;')
after = after.replace('glass+rim*.65+arrow+halo*.6',
    'glass+ring*.34+rings*.22+axes*.22+ticks*.34+rim*.65+arrow+sector*.18+beam*.18+halo*.6')
disk = Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD/M_SovRadarReticle.uasset'
shutil.copy2(disk, out/'M_SovRadarReticle.before.uasset')
(out/'radar.before.hlsl').write_text(before)
shape.set_editor_property('code', after)
edit.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
(out/'radar.after.hlsl').write_text(after)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'radar-translucency.json').write_text(json.dumps(dict(
    status='saved_requires_rendered_review', backing_before=.96, backing_after=.62,
    contact_brushes_changed=False, sweep_binding_changed=False), indent=2))
