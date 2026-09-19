import unreal, os, shutil
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root='/Game/Aurelion/UI/HUD/'
edit=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
assert not unreal.EditorAssetLibrary.does_asset_exist(root+'M_SovRadarContact')
shutil.copy2(str(Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD/WBP_SovHolographicHUD.uasset'),str(out/'HUD-before-contacts.uasset'))
mat=assets.create_asset('M_SovRadarContact',root.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
shape=edit.create_material_expression(mat,unreal.MaterialExpressionCustom,0,0)
shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
shape.set_editor_property('code','float d=length(UV-.5); float aa=max(fwidth(d),.025); float core=1-smoothstep(.23,.23+aa,d); float ring=(1-smoothstep(.28,.28+aa,d))*smoothstep(.15-aa,.15,d); float glow=exp(-d*d*20)*.35; float a=lerp(ring,core,Live)+glow; return float4(1,1,1,saturate(a));')
inputs=[]
for name in ['UV','Live']:
    i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
shape.set_editor_property('inputs',inputs)
uv=edit.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-300,0)
scalar=edit.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-300,160)
scalar.set_editor_property('parameter_name','Live');scalar.set_editor_property('default_value',1)
assert edit.connect_material_expressions(uv,'',shape,'UV')
assert edit.connect_material_expressions(scalar,'',shape,'Live')
for prop,ch in [('MP_EMISSIVE_COLOR','rgb'),('MP_OPACITY','a')]:
    mask=edit.create_material_expression(mat,unreal.MaterialExpressionComponentMask,300,0 if ch=='rgb' else 150)
    for c in 'rgba':mask.set_editor_property(c,c in ch)
    assert edit.connect_material_expressions(shape,'',mask,'')
    assert edit.connect_material_property(mask,'',getattr(unreal.MaterialProperty,prop))
edit.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
for label,value in [('Live',1),('Memory',0)]:
    mi=assets.create_asset('MI_SovRadarContact'+label,root.rstrip('/'),unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    edit.set_material_instance_parent(mi,mat);edit.set_material_instance_scalar_parameter_value(mi,'Live',value)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mi,only_if_is_dirty=False)
    brush=assets.create_asset('SB_SovRadarContact'+label,root.rstrip('/'),unreal.SlateBrushAsset,unreal.SlateBrushAssetFactory())
    b=unreal.SlateBrush();b.set_editor_property('resource_object',mi);b.set_editor_property('draw_as',unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property('brush',b)
    assert unreal.EditorAssetLibrary.save_loaded_asset(brush,only_if_is_dirty=False)
bp=unreal.load_asset(root+'WBP_SovHolographicHUD')
sub=unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
sub.close_all_editors_for_asset(bp)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorStyleSettings')).set_editor_property('AssetEditorOpenLocation',unreal.AssetEditorOpenLocation.MAIN_WINDOW)
sub.open_editor_for_assets([bp])
