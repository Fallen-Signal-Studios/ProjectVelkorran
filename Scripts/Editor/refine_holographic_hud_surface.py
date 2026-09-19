"""Author procedural Echo/radar brushes and refine the existing HUD tree in place.

Does not replace the Widget Blueprint or edit gameplay graphs. Palette, identity,
ability and contact event bindings are a subsequent editor pass, not simulated here.
"""
import json
import os
import shutil
from pathlib import Path
import unreal

OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
PROJECT = Path(unreal.Paths.project_dir())
PACKAGE = '/Game/Aurelion/UI/HUD'
AUTHOR = unreal.SovWidgetTreeAuthoringLibrary
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
report = {'status': 'running', 'saved': []}

ARC = '''
float x = saturate(UV.x);
// A shallow asymmetric sweep contained inside the shared Echo rectangle.
float curve = 0.54 + 0.92*x - 1.20*x*x;
float d = abs(UV.y - curve);
float aa = max(fwidth(UV.y)*1.2, 0.002);
float core = 1.0-smoothstep(0.022, 0.022+aa, d);
float segment = smoothstep(0.055,0.085,frac(x*56.0));
float active = step(x, saturate(Fill));
float fine = 1.0-smoothstep(0.006,0.006+aa,abs(UV.y-curve-0.074));
float glow = exp(-d*72.0)*0.10;
return saturate(core*segment*lerp(0.10,0.94,active)+fine*0.23+glow*active);
'''
RADAR = '''
float2 p = (UV-0.5)*2.0;
float r = length(p);
float aa = max(fwidth(r), 0.002);
float outer = 1.0-smoothstep(0.004,0.004+aa,abs(r-0.83));
float inner = 1.0-smoothstep(0.002,0.002+aa,abs(r-0.42));
float axes = (1.0-smoothstep(0.002,0.002+aa,min(abs(p.x),abs(p.y))))*step(r,0.80);
float needle = step(abs(p.x),0.022)*step(-0.028,p.y)*step(p.y,0.045);
return saturate(outer*0.42+inner*0.11+axes*0.10+needle*0.90);
'''

def backup(path):
    disk = PROJECT / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    if disk.exists():
        dest = OUT / 'Backups' / disk.relative_to(PROJECT)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(disk, dest)

def material(name, code):
    path = PACKAGE + '/' + name
    backup(path)
    mat = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
    assert mat
    EDIT.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    fill = EDIT.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 150)
    fill.set_editor_property('parameter_name', 'Fill')
    fill.set_editor_property('default_value', 0.0)
    shape = EDIT.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    shape.set_editor_property('code', code)
    shape.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for name in ('UV', 'Fill'):
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    shape.set_editor_property('inputs', inputs)
    assert EDIT.connect_material_expressions(uv, '', shape, 'UV')
    assert EDIT.connect_material_expressions(fill, '', shape, 'Fill')
    white = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, -150)
    white.set_editor_property('constant', unreal.LinearColor(1, 1, 1, 1))
    assert EDIT.connect_material_property(white, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert EDIT.connect_material_property(shape, '', unreal.MaterialProperty.MP_OPACITY)
    EDIT.recompile_material(mat)
    assert ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    report['saved'].append(path)
    return mat

def color(r, g, b, a=1):
    return unreal.LinearColor(r, g, b, a)


try:
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    bp_path = PACKAGE + '/WBP_SovHolographicHUD'
    backup(bp_path)
    bp = unreal.load_asset(bp_path)
    assert bp
    names = ['PlateRegion','AmmoRegion','ArcRegion','RadarRegion','HealthBar','ShieldBar','StaminaBar','EchoBar','AmmoText','ArcFill','RadarDisc']
    widgets = {name: AUTHOR.find_widget_in_tree(bp, name) for name in names}
    assert all(widgets.values())
    arc_material = material('M_SovEchoSegmentedArc', ARC)
    radar_material = material('M_SovRadarReticle', RADAR)
    # Keep the current tree intact. Adding/removing named children through the
    # authoring helper triggers reinstancing; graph/tree surgery belongs in UMG.
    for name in ('PlateRegion', 'ArcRegion', 'RadarRegion'):
        widgets[name].set_editor_property('brush_color', color(1, 1, 1, 0))
        widgets[name].set_editor_property('padding', unreal.Margin(0, 0, 0, 0))
    plate_bars = AUTHOR.find_widget_in_tree(bp, 'PlateBars')
    plate_bars.slot.set_editor_property('padding', unreal.Margin(110, 22, 110, 35))
    for name, weight, tint in [('ShieldBar', .24, color(.32,.78,.94)),
                               ('HealthBar', .64, color(.88,.96,1)),
                               ('StaminaBar', .12, color(.48,.65,.72))]:
        bar = widgets[name]
        bar.slot.set_editor_property('size', unreal.SlateChildSize(weight, unreal.SlateSizeRule.FILL))
        bar.slot.set_editor_property('padding', unreal.Margin(0, 2, 0, 2))
        style = bar.get_editor_property('widget_style')
        for field, brush_tint in [('background_image', color(.012,.022,.029,.80)), ('fill_image', color(1,1,1,1))]:
            brush = unreal.SlateBrush()
            brush.set_editor_property('tint_color', unreal.SlateColor(brush_tint))
            brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
            style.set_editor_property(field, brush)
        bar.set_editor_property('widget_style', style)
        bar.set_editor_property('fill_color_and_opacity', tint)
        bar.set_editor_property('percent', 0)
    image = widgets['ArcFill']
    image.set_brush_from_material(arc_material)
    image.slot.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
    image.slot.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
    # The material is the Echo readout; the old rectangular overlay obscured it.
    widgets['EchoBar'].set_visibility(unreal.SlateVisibility.COLLAPSED)
    disc = widgets['RadarDisc']
    disc.set_brush_from_material(radar_material)
    disc.set_editor_property('color_and_opacity', color(.32,.78,.94,.8))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    report['tree'] = list(AUTHOR.describe_widget_tree(bp))
    report['bindings'] = list(AUTHOR.describe_widget_bindings(bp))
    assert len(report['bindings']) == 10 and all('typeMatches=1' in row for row in report['bindings'])
    assert ASSETS.save_loaded_asset(bp, only_if_is_dirty=False)
    report['saved'].append(bp_path)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report['status'] = 'authored_pending_visual_and_event_binding_qualification'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    (OUT / 'hud-refinement.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('HUD_REFINEMENT ' + report['status'])
