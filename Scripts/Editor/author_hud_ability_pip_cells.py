"""Historical one-time material/tree addition. The saved Blueprint owns the separately authored update graph; this script does not reconstruct it."""
import os, json, shutil, unreal
from pathlib import Path
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Aurelion/UI/HUD/'
author = unreal.SovWidgetTreeAuthoringLibrary
edit = unreal.MaterialEditingLibrary
bp = unreal.load_asset(root+'WBP_SovHolographicHUD')
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
shutil.copy2(str(Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD/WBP_SovHolographicHUD.uasset'),out/'WBP_SovHolographicHUD.before.uasset')
assert not unreal.EditorAssetLibrary.does_asset_exist(root+'M_SovAbilityPips')
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SovAbilityPips',root.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
shape = edit.create_material_expression(mat,unreal.MaterialExpressionCustom,0,0)
shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
shape.set_editor_property('code',r'''
// The six cells occupy the existing plate shelf; parameters are gameplay-fed.
float values[6] = {Pip0,Pip1,Pip2,Pip3,Pip4,Pip5};
float u=(UV.x-.325)/.35;
float y=(UV.y-.873)/.085;
float index=clamp(floor(u*6),0,5);
float2 p=float2(frac(u*6),y);
float aa=max(fwidth(y),.015);
float d=max(abs(p.x-.5)-.405,abs(p.y-.5)-.46);
float body=1-smoothstep(0,aa,d);
float rim=body*(1-smoothstep(.08,.08+aa,abs(d+.065)));
float v=values[(int)index];
float ready=step(.75,v);
float active=step(1.5,v);
float done=ready>0?1:saturate((v-.001)/.499);
float fill=body*step(p.x,.095+.81*done)*step(0,v);
float inner=(1-smoothstep(.32,.32+aa,abs(p.y-.5)))*step(.16,p.x)*step(p.x,.84);
float stripe=active*(1-smoothstep(.035,.035+aa,abs(p.y-.5)))*body;
float brightness=.01*body+rim*lerp(.4,.95,ready)+fill*inner*lerp(.32,.95,ready)+stripe*.6;
float mask=step(0,u)*step(u,1)*step(0,y)*step(y,1);
return float4(Accent.rgb*brightness,mask*saturate(body*.2+rim*.8+fill*inner*.9+stripe));
''')
inputs=[]
for name in ['UV','Accent']+['Pip'+str(i) for i in range(6)]:
    ci=unreal.CustomInput(); ci.set_editor_property('input_name',name); inputs.append(ci)
shape.set_editor_property('inputs',inputs)
uv=edit.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-500,0)
assert edit.connect_material_expressions(uv,'',shape,'UV')
accent=edit.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-500,160)
accent.set_editor_property('parameter_name','Accent')
accent.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
assert edit.connect_material_expressions(accent,'',shape,'Accent')
for i in range(6):
    scalar=edit.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-500,300+i*100)
    scalar.set_editor_property('parameter_name','Pip'+str(i)); scalar.set_editor_property('default_value',-1.0)
    assert edit.connect_material_expressions(scalar,'',shape,'Pip'+str(i))
for field,channel in [('MP_EMISSIVE_COLOR','rgb'),('MP_OPACITY','a')]:
    mask=edit.create_material_expression(mat,unreal.MaterialExpressionComponentMask,300,0 if channel=='rgb' else 150)
    for c in 'rgba':mask.set_editor_property(c,c in channel)
    assert edit.connect_material_expressions(shape,'',mask,'')
    assert edit.connect_material_property(mask,'',getattr(unreal.MaterialProperty,field))
edit.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
parent=author.find_widget_in_tree(bp,'PlateBars').get_parent()
assert isinstance(parent,unreal.Overlay)
assert author.add_widget_to_tree(bp,unreal.Image,'AbilityPips',parent)
# Adding a variable recompiles the Blueprint, so re-read all references.
pips=author.find_widget_in_tree(bp,'AbilityPips')
pips.set_brush_from_material(mat)
pips.set_color_and_opacity(unreal.LinearColor(1,1,1,1))
pips.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
pips.slot.set_editor_property('horizontal_alignment',unreal.HorizontalAlignment.H_ALIGN_FILL)
pips.slot.set_editor_property('vertical_alignment',unreal.VerticalAlignment.V_ALIGN_FILL)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert len(author.describe_widget_bindings(bp))==10
assert all('typeMatches=1' in x for x in author.describe_widget_bindings(bp))
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
task=unreal.AssetExportTask()
for k,v in dict(object=bp,exporter=unreal.ObjectExporterT3D(),filename=str(out/'hud-pips-before-graph.t3d'),automated=True,prompt=False,selected=False,replace_identical=False).items():task.set_editor_property(k,v)
assert unreal.Exporter.run_asset_export_task(task)
(out/'pip-authoring.json').write_text(json.dumps(dict(status='cells_authored_bindings_pending',bindings=list(author.describe_widget_bindings(bp)),tree=list(author.describe_widget_tree(bp))),indent=2))
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorStyleSettings'))
settings.set_editor_property('AssetEditorOpenLocation',unreal.AssetEditorOpenLocation.MAIN_WINDOW)
assert unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([bp])

