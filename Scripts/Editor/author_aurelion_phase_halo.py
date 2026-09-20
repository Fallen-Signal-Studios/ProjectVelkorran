"""Independent, world-occluded phase protection geometry; no mesh-overlay ownership changes."""
import json,os,shutil
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assets=unreal.EditorAssetLibrary
path='/Game/Aurelion/Enemies/Materials/M_AurelionPhaseHalo'
assert not assets.does_asset_exist(path),'Review existing halo instead of overwriting it'
cue=unreal.load_asset('/Game/Cues/Aurelion/GC_AurelionLethalFloor')
shutil.copy2(Path(unreal.Paths.project_dir())/'Content/Cues/Aurelion/GC_AurelionLethalFloor.uasset',out/'cue.before.uasset')
mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AurelionPhaseHalo','/Game/Aurelion/Enemies/Materials',unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
edit=unreal.MaterialEditingLibrary
uv=edit.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
shape=edit.create_material_expression(mat,unreal.MaterialExpressionCustom,-300,0)
pin=unreal.CustomInput();pin.set_editor_property('input_name','UV')
shape.set_editor_property('inputs',[pin])
shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
shape.set_editor_property('code','''
float ringA=1-smoothstep(.004,.01,abs(UV.y-.34));
float ringB=1-smoothstep(.004,.01,abs(UV.y-.68));
float segments=1-smoothstep(.78,.85,frac(UV.x*12));
float meridian=1-smoothstep(.008,.022,abs(frac(UV.x*4)-.5));
float ends=smoothstep(.13,.19,UV.y)*(1-smoothstep(.81,.87,UV.y));
return saturate(max(max(ringA,ringB)*segments,meridian*ends*.45))*.58;
''')
assert edit.connect_material_expressions(uv,'',shape,'UV')
assert edit.connect_material_property(shape,'',unreal.MaterialProperty.MP_OPACITY)
color=edit.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,150)
color.set_editor_property('constant',unreal.LinearColor(.75,.3,2.2,1))
assert edit.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(mat)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib=unreal.SubobjectDataBlueprintFunctionLibrary
handles=list(sub.k2_gather_subobject_data_for_blueprint(cue))
roots=[h for h in handles if lib.is_root_actor(lib.get_data(h))]
assert len(roots)==1
handle,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=roots[0],new_class=unreal.StaticMeshComponent,blueprint_context=cue))
assert lib.is_handle_valid(handle),str(reason)
assert sub.rename_subobject(handle,unreal.Text('PhaseProtectionHalo'))
mesh=lib.get_object(lib.get_data(handle))
mesh.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'))
mesh.set_material(0,mat)
mesh.set_editor_property('relative_scale3d',unreal.Vector(2.15,2.15,2.6))
mesh.set_editor_property('relative_location',unreal.Vector(0,0,0))
mesh.set_mobility(unreal.ComponentMobility.MOVABLE)
mesh.set_collision_profile_name('NoCollision')
mesh.set_editor_property('generate_overlap_events',False)
mesh.set_cast_shadow(False)
mesh.set_editor_property('receives_decals',False)
cdo=unreal.get_default_object(cue.generated_class())
cdo.set_editor_property('auto_attach_to_owner',True)
assert cdo.get_editor_property('auto_destroy_on_remove')
assert cdo.get_editor_property('auto_destroy_delay')==0
unreal.BlueprintEditorLibrary.compile_blueprint(cue)
assert assets.save_loaded_asset(mat,False)
assert assets.save_loaded_asset(cue,False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'phase-halo-author.json').write_text(json.dumps(dict(status='saved_requires_runtime_review',cue=cue.get_path_name(),material=mat.get_path_name(),component=mesh.get_path_name()),indent=2))
