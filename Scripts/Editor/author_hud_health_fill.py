"""Give the existing native-driven health bar a clipped, beveled UI brush.

Preserves widget hierarchy and palette property bindings. The progress bar owns
the health fraction; mask mode clips the full-width shape without stretching it.
"""
import json
import os
import shutil
from pathlib import Path
import unreal

OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
ROOT = '/Game/Aurelion/UI/HUD/'
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
AUTHOR = unreal.SovWidgetTreeAuthoringLibrary
NAME = 'M_SovHealthBeveledFill'
CODE = r'''
// Normalized bevels retain their silhouette as Slate clips the depleted portion.
float2 p = UV;
float aa = max(fwidth(p.y), .002);
float side = min(p.x, 1-p.x);
float edge = min(min(p.y, 1-p.y), side*22-abs(p.y-.5)*.65);
float body = smoothstep(0, aa, edge);
float rim = (1-smoothstep(.025,.025+aa,edge))*body;
float upper = exp(-abs(p.y-.09)*32)*.18;
float lower = exp(-abs(p.y-.89)*24)*.07;
float energy = lerp(.30,.98,smoothstep(0,1,p.x));
float depth = lerp(.72,1.0,1-abs(p.y-.5)*2);
float core = exp(-abs(p.y-.46)*12)*.09;
float brightness = saturate(energy*depth+upper+lower+core+rim*.65);
return float4(brightness.xxx, body);
'''

assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
for package in (ROOT+NAME, ROOT+'WBP_SovHolographicHUD'):
    source = Path(unreal.Paths.project_dir())/'Content'/(package.removeprefix('/Game/')+'.uasset')
    if source.exists():
        target = OUT/'Backups'/source.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

material = unreal.load_asset(ROOT+NAME) if ASSETS.does_asset_exist(ROOT+NAME) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, ROOT.rstrip('/'), unreal.Material, unreal.MaterialFactoryNew())
EDIT.delete_all_material_expressions(material)
material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
uv = EDIT.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -500, 0)
shape = EDIT.create_material_expression(material, unreal.MaterialExpressionCustom, -250, 0)
shape.set_editor_property('code', CODE)
shape.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
input_uv = unreal.CustomInput()
input_uv.set_editor_property('input_name', 'UV')
shape.set_editor_property('inputs', [input_uv])
assert EDIT.connect_material_expressions(uv, '', shape, 'UV')
for channels, prop, y in [('rgb', unreal.MaterialProperty.MP_EMISSIVE_COLOR, 0), ('a', unreal.MaterialProperty.MP_OPACITY, 150)]:
    mask = EDIT.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, y)
    for channel in 'rgba':
        mask.set_editor_property(channel, channel in channels)
    assert EDIT.connect_material_expressions(shape, '', mask, '')
    assert EDIT.connect_material_property(mask, '', prop)
EDIT.recompile_material(material)
assert ASSETS.save_loaded_asset(material, only_if_is_dirty=False)

bp = unreal.load_asset(ROOT+'WBP_SovHolographicHUD')
bar = AUTHOR.find_widget_in_tree(bp, 'HealthBar')
style = bar.get_editor_property('widget_style')
for field, tint in [('fill_image', unreal.LinearColor(1,1,1,1)), ('background_image', unreal.LinearColor(.018,.025,.032,.85))]:
    brush = unreal.SlateBrush()
    brush.set_editor_property('resource_object', material)
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property('tint_color', unreal.SlateColor(tint))
    style.set_editor_property(field, brush)
bar.set_editor_property('widget_style', style)
bar.set_editor_property('bar_fill_style', unreal.ProgressBarFillStyle.MASK)
bar.set_editor_property('border_padding', unreal.Vector2D(0,0))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
bindings = list(AUTHOR.describe_widget_bindings(bp))
assert len(bindings)==10 and all('typeMatches=1' in row for row in bindings)
assert ASSETS.save_loaded_asset(bp, only_if_is_dirty=False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(OUT/'health-fill.json').write_text(json.dumps({'status':'saved_pending_render_qualification', 'material':material.get_path_name(), 'fill_style':str(bar.get_editor_property('bar_fill_style')), 'bindings':bindings}, indent=2))
unreal.log('HUD_HEALTH_BEVEL_SAVED')
