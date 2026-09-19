"""Prepare a view-driven radar sweep parameter; no autonomous clock or fake contacts."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
name = 'M_SovRadarReticle'
src = Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD'/(name+'.uasset')
backup = out/(name+'.before.uasset')
if not backup.exists():
    shutil.copy2(src, backup)
mat = unreal.load_asset('/Game/Aurelion/UI/HUD/'+name)
edit = unreal.MaterialEditingLibrary
owned = []
visited = set()
def visit(node):
    if not node or node.get_path_name() in visited:
        return
    visited.add(node.get_path_name())
    if isinstance(node, unreal.MaterialExpressionCustom):
        owned.append(node)
    for upstream in edit.get_inputs_for_material_expression(mat, node):
        visit(upstream)
visit(edit.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR))
assert len(owned) == 1, 'Expected one connected radar shape expression'
shape = owned[0]
inputs = list(shape.get_editor_property('inputs'))
if not any(str(i.get_editor_property('input_name')) == 'SweepSeconds' for i in inputs):
    value = unreal.CustomInput()
    value.set_editor_property('input_name','SweepSeconds')
    inputs.append(value)
    shape.set_editor_property('inputs',inputs)
    parameter = edit.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-600,300)
    parameter.set_editor_property('parameter_name','SweepSeconds')
    parameter.set_editor_property('default_value',0.0)
    assert edit.connect_material_expressions(parameter,'',shape,'SweepSeconds')
shape.set_editor_property('code',r'''
float2 p = (UV-.5)*2;
float r = length(p);
float aa = max(fwidth(r),.002);
float body = 1-smoothstep(.835,.835+aa,r);
float ring = 1-smoothstep(.004,.004+aa,abs(r-.83));
float rim = 1-smoothstep(.010,.010+aa,abs(r-.87));
float rings = (1-smoothstep(.002,.002+aa,min(abs(r-.56),abs(r-.28))))*body;
float axes = (1-smoothstep(.002,.002+aa,min(abs(p.x),abs(p.y))))*body;
float ticks = step(.775,r)*step(r,.84)*(1-smoothstep(.011,.014,min(abs(p.x),abs(p.y))));
float arrow = step(-.105,p.y)*step(p.y,.068)*step(abs(p.x),(.105+p.y)*.46);
arrow *= 1-step(.033,p.y)*step(abs(p.x),(p.y-.033)*1.8);
// Match the presentation view's clockwise 70 degrees/second sweep.
float angle = atan2(p.y,p.x)/6.28318530718;
float behind = frac(SweepSeconds*(70.0/360.0)-angle);
float sector = pow(saturate(1-behind/.14),2.5)*body*smoothstep(.08,.18,r);
float beam = (1-smoothstep(.0015,.005,behind))*body*smoothstep(.08,.16,r);
float brightness = body*.012+ring*.62+rim*.30+rings*.12+axes*.10+ticks*.9+arrow+sector*.30+beam*.55;
return float4(brightness.xxx,saturate(body*.83+rim*.5+arrow));
''')
edit.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'radar-sweep.json').write_text(json.dumps({'status':'material_saved_requires_blueprint_view_binding','parameter':'SweepSeconds'},indent=2))
unreal.log('HUD_RADAR_SWEEP_MATERIAL_SAVED')
