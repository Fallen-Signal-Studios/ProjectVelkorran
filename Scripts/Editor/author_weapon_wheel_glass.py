"""Author the project-owned radial menu's glass skin; preserve all selection graphs and vendor assets."""
import json,os,shutil
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
project=Path(unreal.Paths.project_dir())
edit=unreal.MaterialEditingLibrary
assets=unreal.EditorAssetLibrary
author=unreal.SovWidgetTreeAuthoringLibrary
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
# CommonActionWidget can first resolve these while compiling its owner. Load them beforehand.
for controller in ('ControllerData_PC_KB','ControllerData_PC_Gamepad_Xbox'):
    assert unreal.load_class(None,'/NarrativePro/Pro/Core/UI/ControllerData/'+controller+'.'+controller+'_C')
root='/Game/Aurelion/UI/HUD'
name='M_SovWeaponWheelGlass'
code=r'''
float2 p=(UV-.5)*2;
float angle=atan2(p.y,p.x);
float r=length(p);
// The Dominion lens has shallow machined facets; Reformation keeps a continuous optical curve.
float facet=cos(fmod(angle+6.2831853,6.2831853/24)-3.14159265/24)/cos(3.14159265/24);
r*=lerp(1,facet,saturate(DominionFrame));
float aa=max(fwidth(r),.002);
float ring=smoothstep(.47,.47+aa,r)*(1-smoothstep(.92,.92+aa,r));
float sector=abs(frac((-angle/6.2831853)*max(SectorCount,1)+.5)-.5);
float seam=smoothstep(.485,.495,sector)*ring;
float body=ring*(1-seam);
float rim=(1-smoothstep(.002,.002+aa,min(abs(r-.916),abs(r-.478))))*body;
float bead=(1-smoothstep(.003,.003+aa,abs(r-.889)))*body;
float outer=(1-smoothstep(.001,.001+aa,abs(r-.954)));
float inner=(1-smoothstep(.001,.001+aa,abs(r-.433)));
float well=1-smoothstep(.414,.414+aa,r);
float ticks=step(.965,r)*step(r,.982)*step(.92,frac(angle/6.2831853*72));
float selected=saturate(ActiveMask)*body;
float selectedLip=selected*(1-smoothstep(.008,.008+aa,abs(r-.913)));
float cursor=saturate(SearchMask)*(1-smoothstep(.002,.002+aa,abs(r-.453)));
if(HighContrast>.5) {
 float white=saturate(rim*.7+bead*selected+selectedLip+outer*.3+inner*.4+ticks*.4+cursor);
 return float4(white.xxx,saturate(body+well+outer+inner+ticks+cursor));
}
// Static curved highlights suggest depth without shimmer or autonomous motion.
float fresnel=pow(saturate((r-.48)/.44),5);
float light=pow(saturate(dot(normalize(p+float2(.0001,0)),normalize(float2(-.6,-1)))),8);
float reflection=exp(-pow((p.y+.32+p.x*.24)/.095,2))*body;
float3 accent=ProtagonistAccent.rgb;
float3 glass=lerp(float3(.012,.021,.035),accent*.12,selected*.65);
glass+=accent*(fresnel*.07+rim*.55+bead*(.05+selected*.19)+selectedLip*.9+cursor*.5);
glass+=float3(.65,.84,1)*(light*fresnel*.45+reflection*.08)*body;
glass+=accent*(outer*.17+inner*.2+ticks*.23);
glass+=float3(.005,.013,.022)*well;
float alpha=body*(.55+selected*.15+rim*.3+light*fresnel*.2)+well*.96+outer*.35+inner*.36+ticks*.5+cursor*.7;
return float4(glass,saturate(alpha));
'''

def backup(path):
    disk=project/'Content'/(path.removeprefix('/Game/')+'.uasset')
    if disk.exists():shutil.copy2(disk,out/(disk.stem+'.before.uasset'))

path=root+'/'+name
backup(path)
mat=unreal.load_asset(path) if assets.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(mat)
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
uv=edit.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-800,0)
nodes={'UV':uv}
for i,(key,value) in enumerate({'SectorCount':4.,'ActiveAngle':0.,'SearchAngle':0.,'HighContrast':0.,'DominionFrame':0.}.items()):
    node=edit.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-800,120+i*100)
    node.set_editor_property('parameter_name',key);node.set_editor_property('default_value',value);nodes[key]=node
accent=edit.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-800,800)
accent.set_editor_property('parameter_name','ProtagonistAccent');accent.set_editor_property('default_value',unreal.LinearColor(.62,.87,1,1));nodes['ProtagonistAccent']=accent
for i,(mask,param) in enumerate([('ActiveMask','ActiveAngle'),('SearchMask','SearchAngle')]):
    function=edit.create_material_expression(mat,unreal.MaterialExpressionMaterialFunctionCall,-550,200+i*200)
    function.set_editor_property('material_function',unreal.load_asset('/NarrativePro/Pro/Core/UI/Menus/RadialMenus/Materials/MF_Sector'))
    assert edit.connect_material_expressions(nodes[param],'',function,'SectorRotation')
    assert edit.connect_material_expressions(nodes['SectorCount'],'',function,'SectorCount')
    nodes[mask]=function
shape=edit.create_material_expression(mat,unreal.MaterialExpressionCustom,-250,0)
names=('UV','SectorCount','ProtagonistAccent','HighContrast','DominionFrame','ActiveMask','SearchMask')
inputs=[]
for key in names:
    entry=unreal.CustomInput();entry.set_editor_property('input_name',key);inputs.append(entry)
shape.set_editor_property('inputs',inputs)
shape.set_editor_property('code',code);shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
for key in names:assert edit.connect_material_expressions(nodes[key],'',shape,key)
for channels,prop in [('rgb',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('a',unreal.MaterialProperty.MP_OPACITY)]:
    mask=edit.create_material_expression(mat,unreal.MaterialExpressionComponentMask,0,0)
    for channel in 'rgba':mask.set_editor_property(channel,channel in channels)
    assert edit.connect_material_expressions(shape,'',mask,'')
    assert edit.connect_material_property(mask,'',prop)
edit.recompile_material(mat)
assert assets.save_loaded_asset(mat,False)

glyph_name='M_SovWeaponWheelGlyph'
glyph_path=root+'/'+glyph_name;backup(glyph_path)
glyph=unreal.load_asset(glyph_path) if assets.does_asset_exist(glyph_path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(glyph_name,root,unreal.Material,unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(glyph)
glyph.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
glyph.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
texture=edit.create_material_expression(glyph,unreal.MaterialExpressionTextureSampleParameter2D,-500,0)
texture.set_editor_property('parameter_name','WeaponTexture')
texture.set_editor_property('texture',unreal.load_asset('/NarrativePro/Pro/Core/UI/Textures/Generic/T_Tick'))
tint=edit.create_material_expression(glyph,unreal.MaterialExpressionVectorParameter,-500,250)
tint.set_editor_property('parameter_name','ProtagonistAccent');tint.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
silhouette=edit.create_material_expression(glyph,unreal.MaterialExpressionCustom,-200,0)
glyph_inputs=[]
for key in ('IconRGB','IconAlpha','ProtagonistAccent'):
    entry=unreal.CustomInput();entry.set_editor_property('input_name',key);glyph_inputs.append(entry)
silhouette.set_editor_property('inputs',glyph_inputs)
silhouette.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
silhouette.set_editor_property('code','return float4(ProtagonistAccent.rgb * (.8 + .2 * dot(IconRGB,float3(.2126,.7152,.0722))), IconAlpha);')
assert edit.connect_material_expressions(texture,'RGB',silhouette,'IconRGB')
assert edit.connect_material_expressions(texture,'A',silhouette,'IconAlpha')
assert edit.connect_material_expressions(tint,'',silhouette,'ProtagonistAccent')
for channels,prop in [('rgb',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('a',unreal.MaterialProperty.MP_OPACITY)]:
    mask=edit.create_material_expression(glyph,unreal.MaterialExpressionComponentMask,0,0)
    for channel in 'rgba':mask.set_editor_property(channel,channel in channels)
    assert edit.connect_material_expressions(silhouette,'',mask,'')
    assert edit.connect_material_property(mask,'',prop)
edit.recompile_material(glyph)
assert assets.save_loaded_asset(glyph,False)

base='/Game/UI/Narrative/Menus/RadialMenus/'
bp=unreal.load_asset(base+'W_NarrativeMenu_RadialMenuBase');backup(base+bp.get_name())
assert author.replace_material_factory_parent(bp,unreal.load_asset('/NarrativePro/Pro/Core/UI/Menus/RadialMenus/Materials/MI_Radial'),mat)
image=author.find_widget_in_tree(bp,'Image_WeaponWheel')
image.set_brush_from_material(mat)
image.set_brush_size(unreal.Vector2D(600,600))
# The optical lens supplies its own backing; retire the stock full-screen grid and shadow.
border=author.find_widget_in_tree(bp,'CommonBorder')
border.set_style(None)
border.set_brush_color(unreal.LinearColor(.005,.011,.02,1))
# Keep a subdued veil if a designer deliberately re-enables this retired layer.
border.set_render_opacity(.27)
border.set_visibility(unreal.SlateVisibility.COLLAPSED)
brush=unreal.SlateBrush();brush.set_editor_property('draw_as',unreal.SlateBrushDrawType.IMAGE)
border.set_brush(brush)
author.find_widget_in_tree(bp,'CommonBorder_98').set_visibility(unreal.SlateVisibility.COLLAPSED)
overlay=author.find_widget_in_tree(bp,'Overlay_Root')
lens=author.find_widget_in_tree(bp,'ProtagonistGlassIdentity')
if not lens:lens=author.add_widget_to_tree(bp,unreal.SovWeaponWheelGlass,'ProtagonistGlassIdentity',overlay)
assert lens
lens.set_editor_property('weapon_glyph_material',glyph)
lens.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
lens.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
lens.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
title=author.find_widget_in_tree(bp,'CommonTextBlock_RadialTitle')
white=unreal.load_class(None,'/NarrativePro/Pro/Core/UI/Style/MasterStyles/Text/White/TextStyle_Master_White_H3.TextStyle_Master_White_H3_C')
title.set_style(white)
title.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(1,1,1,1)))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp,False)
center=unreal.load_asset(base+'WBP_RadialLRCenter');backup(base+center.get_name())
for key in ('Text_Header','TextBlock_Instruction','TextBlock_Mainhand','TextBlock_Offhand'):
    text=author.find_widget_in_tree(center,key)
    if text:
        font=text.get_editor_property('font')
        font.set_editor_property('size',22 if key=='Text_Header' else 16)
        font.set_editor_property('typeface_font_name','Bold' if key=='Text_Header' else 'Regular')
        text.set_style(None)
        text.set_font(font)
        text.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.87,.94,1,1)))
        if key=='TextBlock_Instruction':text.slot.set_padding(unreal.Margin(0,6,0,0))
unreal.BlueprintEditorLibrary.compile_blueprint(center)
assert assets.save_loaded_asset(center,False)
wheel=unreal.load_asset(base+'WM_WeaponWheel_LR')
backup(base+wheel.get_name())
unreal.BlueprintEditorLibrary.compile_blueprint(wheel)
assert assets.save_loaded_asset(wheel,False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'weapon-wheel-glass.json').write_text(json.dumps({'status':'saved_requires_gameplay_review','material':mat.get_path_name(),'tree':list(author.describe_widget_tree(bp))},indent=2))
