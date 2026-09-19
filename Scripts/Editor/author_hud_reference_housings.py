"""Refine existing HUD material silhouettes without replacing its widget tree."""
import json
import os
import shutil
from pathlib import Path
import unreal

ROOT = '/Game/Aurelion/UI/HUD/'
OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'ReferenceHousings'
OUT.mkdir(parents=True, exist_ok=True)
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
AUTHOR = unreal.SovWidgetTreeAuthoringLibrary

ARC = r'''
float x = saturate(UV.x);
float curve = 0.54 + 0.92*x - 1.20*x*x;
float d = abs(UV.y-curve);
float aa = max(fwidth(UV.y), .002);
float edge = min(x,1-x);
float h = min(.108,edge*4.0);
float split = smoothstep(.018,.023,abs(x-.5));
float body = (1-smoothstep(h,h+aa,d))*split;
float rim = (1-smoothstep(.009,.009+aa,abs(d-h+.016)))*body;
float inset = (1-smoothstep(.053,.053+aa,d))*body;
// Four broad cells on either side; the middle stays open for paired chevrons.
float cell = smoothstep(.022,.030,frac(x*8));
float ready = step(1-abs(x-.5)*2,saturate(Fill));
float lit = inset*cell*lerp(.065,.90,ready);
float chevronX = abs(x-.5);
float chev = (1-smoothstep(.003,.005,abs(chevronX-(.012+abs(UV.y-curve)*.075))))*step(d,.063);
float fine = (1-smoothstep(.004,.004+aa,abs(UV.y-curve-.17)))*.22;
float glow = exp(-abs(d-h+.016)*90)*.10*split;
float brightness = .018*body + rim*.60 + lit + chev*.86 + fine + glow;
return float4(brightness.xxx,saturate(body*.84+chev+fine+glow));
'''

RADAR = r'''
float2 p = (UV-.5)*2;
float r = length(p);
float aa = max(fwidth(r),.002);
float body = 1-smoothstep(.835,.835+aa,r);
float ring = 1-smoothstep(.004,.004+aa,abs(r-.83));
float rim = 1-smoothstep(.010,.010+aa,abs(r-.87));
float rings = (1-smoothstep(.002,.002+aa,min(abs(r-.56),abs(r-.28))))*body;
float axes = (1-smoothstep(.002,.002+aa,min(abs(p.x),abs(p.y))))*body;
float ticks = step(.775,r)*step(r,.84)*(1-smoothstep(.011,.014,min(abs(p.x),abs(p.y))));
// Forward-facing player arrow, with an inset notch at the tail.
float arrow = step(-.105,p.y)*step(p.y,.068)*step(abs(p.x),(.105+p.y)*.46);
arrow *= 1-step(.033,p.y)*step(abs(p.x),(p.y-.033)*1.8);
float brightness = body*.012+ring*.62+rim*.30+rings*.12+axes*.10+ticks*.9+arrow;
return float4(brightness.xxx,saturate(body*.83+rim*.5+arrow));
'''

PLATE = r'''
float2 p = UV;
float aa = max(fwidth(p.y),.002);
// Hexagonal health housing, with separate upper shield and lower pip shelves.
float halfW = .49-abs(p.y-.59)*.08;
float dist = max(abs(p.x-.5)-halfW,abs(p.y-.59)-.235);
float body = 1-smoothstep(0,aa,dist);
float rim = (1-smoothstep(.008,.008+aa,abs(dist+.021)))*body;
float cap = step(abs(p.x-.5),.22)*step(.015,p.y)*step(p.y,.16);
float shelf = step(abs(p.x-.5),.24)*step(.86,p.y)*step(p.y,.98);
float shield = step(abs(p.x-.5),.44)*step(.19,p.y)*step(p.y,.31);
float brightness = .015*(body+cap+shelf+shield)+rim*.65;
return float4(brightness.xxx,saturate((body+cap+shelf+shield)*.83));
'''

AMMO = r'''
float aa=max(fwidth(UV.y),.002);
float left=.04+max(.24-UV.y,0)*.14;
float right=.84+UV.y*.13;
float d=max(max(left-UV.x,UV.x-right),max(.06-UV.y,UV.y-.90));
float body=1-smoothstep(0,aa,d);
float rim=(1-smoothstep(.009,.009+aa,abs(d+.026)))*body;
return float4((body*.018+rim*.62).xxx,body*.86);
'''

def backup(path):
    src=Path(unreal.Paths.project_dir())/'Content'/(path.removeprefix('/Game/')+'.uasset')
    dst=OUT/(src.stem+'.before.uasset')
    if src.exists() and not dst.exists():
        shutil.copy2(src,dst)

def material(name,code):
    path=ROOT+name
    backup(path)
    mat=unreal.load_asset(path) if ASSETS.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
    EDIT.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=EDIT.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
    fill=EDIT.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-600,150)
    fill.set_editor_property('parameter_name','Fill')
    fill.set_editor_property('default_value',0.0)
    shape=EDIT.create_material_expression(mat,unreal.MaterialExpressionCustom,-300,0)
    shape.set_editor_property('code',code)
    shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs=[]
    for name in ('UV','Fill'):
        item=unreal.CustomInput()
        item.set_editor_property('input_name',name)
        inputs.append(item)
    shape.set_editor_property('inputs',inputs)
    assert EDIT.connect_material_expressions(uv,'',shape,'UV')
    assert EDIT.connect_material_expressions(fill,'',shape,'Fill')
    for channels,prop,y in [('rgb',unreal.MaterialProperty.MP_EMISSIVE_COLOR,0),('a',unreal.MaterialProperty.MP_OPACITY,150)]:
        mask=EDIT.create_material_expression(mat,unreal.MaterialExpressionComponentMask,0,y)
        for channel in 'rgba':
            mask.set_editor_property(channel,channel in channels)
        assert EDIT.connect_material_expressions(shape,'',mask,'')
        assert EDIT.connect_material_property(mask,'',prop)
    EDIT.recompile_material(mat)
    assert ASSETS.save_loaded_asset(mat,only_if_is_dirty=False)
    return mat

bp=unreal.load_asset(ROOT+'WBP_SovHolographicHUD')
backup(ROOT+'WBP_SovHolographicHUD')
material('M_SovEchoSegmentedArc',ARC)
material('M_SovRadarReticle',RADAR)
for widget,name,code in [('PlateRegion','M_SovPlateHousing',PLATE),('AmmoRegion','M_SovAmmoHousing',AMMO)]:
    mat=material(name,code)
    border=AUTHOR.find_widget_in_tree(bp,widget)
    border.set_brush_from_material(mat)
    border.set_editor_property('brush_color',unreal.LinearColor(.32,.78,.94,1))
AUTHOR.find_widget_in_tree(bp,'PlateBars').slot.set_editor_property('padding',unreal.Margin(56,16,56,13))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
bindings=list(AUTHOR.describe_widget_bindings(bp))
assert len(bindings)==10 and all('typeMatches=1' in row for row in bindings)
assert ASSETS.save_loaded_asset(bp,only_if_is_dirty=False)
(OUT/'result.json').write_text(json.dumps({'status':'saved_pending_visual_qualification','native_bindings':bindings},indent=2))
unreal.log('HUD_REFERENCE_HOUSINGS_SAVED')
