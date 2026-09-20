"""Add an optional HighContrast branch while preserving each normal HUD shader verbatim."""
import hashlib,json,os,re,shutil
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
project=Path(unreal.Paths.project_dir())
edit=unreal.MaterialEditingLibrary
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
branches={
 'M_SovPlateHousing':'return float4(saturate(rim+capRim+shelfRim+shieldRim+glyph).xxx,glass);',
 'M_SovAmmoHousing':'return float4(saturate(rim+cartridge).xxx,body);',
 'M_SovEchoSegmentedArc':'return float4(saturate(rim+inset*cell*ready+chev+fine).xxx,saturate(body+chev+fine));',
 'M_SovRadarReticle':'return float4(saturate(ring+rim+rings*.75+axes*.65+ticks+arrow).xxx,saturate(body+rim));',
 'M_SovHealthBeveledFill':'return float4(1,1,1,body);',
 'M_SovAbilityPips':'return float4(saturate(rim+fill*inner+stripe).xxx,mask*body);',
}
pending=[]
for name,branch in branches.items():
    mat=unreal.load_asset('/Game/Aurelion/UI/HUD/'+name)
    seen=set();shapes=[]
    def visit(node):
        if not node or node.get_path_name() in seen:return
        seen.add(node.get_path_name())
        if isinstance(node,unreal.MaterialExpressionCustom):shapes.append(node)
        for upstream in edit.get_inputs_for_material_expression(mat,node):visit(upstream)
    visit(edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    assert len(shapes)==1,name
    shape=shapes[0];before=shape.get_editor_property('code')
    marker='// Accessible flat values and opaque silhouettes; normal shader remains below.\n'
    addition=marker+'if (HighContrast > .5) { '+branch+' }\n'
    if addition in before:continue
    assert 'HighContrast' not in before and before.count('return float4')==1,name
    for symbol in re.findall(r'\b[A-Za-z_]\w*\b',branch):
        if symbol not in ('return','float4','saturate','xxx'):
            assert re.search(r'\b'+symbol+r'\b',before),(name,symbol)
    at=before.index('return float4');after=before[:at]+addition+before[at:]
    pending.append((name,mat,shape,before,after))
report={'saved':[],'status':'running'}
for name,mat,shape,before,after in pending:
    disk=project/'Content/Aurelion/UI/HUD'/(name+'.uasset')
    shutil.copy2(disk,out/(name+'.before.uasset'))
    (out/(name+'.before.hlsl')).write_text(before)
    inputs=list(shape.get_editor_property('inputs'))
    assert not any(str(i.get_editor_property('input_name'))=='HighContrast' for i in inputs)
    value=unreal.CustomInput();value.set_editor_property('input_name','HighContrast');inputs.append(value)
    shape.set_editor_property('inputs',inputs)
    parameter=edit.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-700,900)
    parameter.set_editor_property('parameter_name','HighContrast');parameter.set_editor_property('default_value',0.)
    assert edit.connect_material_expressions(parameter,'',shape,'HighContrast')
    shape.set_editor_property('code',after)
    edit.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
    (out/(name+'.after.hlsl')).write_text(after)
    report['saved'].append({'material':mat.get_path_name(),'normal_shader_sha256':hashlib.sha256(before.encode()).hexdigest()})
report['status']='saved_requires_runtime_and_visual_review'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'hud-high-contrast-materials.json').write_text(json.dumps(report,indent=2))
