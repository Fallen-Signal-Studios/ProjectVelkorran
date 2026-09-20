"""Bind only the owned protagonist camera templates to the compiled shoulder director."""
import unreal,os,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'ShoulderBinding'
out.mkdir(exist_ok=False)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib=unreal.SubobjectDataBlueprintFunctionLibrary
camera=unreal.load_asset('/Game/Aurelion/Camera/CA_SovProtagonist')
bpdir=unreal.load_asset('/Game/Aurelion/Camera/BP_SovCameraDirector')
director=unreal.find_object(camera,'BlueprintCameraDirector_0')
assert director.get_editor_property('CameraDirectorEvaluatorClass')==bpdir.generated_class()
rows=[]
for name in ('Tarrik','Selene'):
 path='/Game/PlayerCharacters/BP_Sov'+name
 bp=unreal.load_asset(path)
 components=[lib.get_object(lib.get_data(h)) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
 components=[o for o in components if isinstance(o,unreal.GameplayCameraComponent)]
 assert len(components)==1
 obj=components[0]
 assert obj.get_path_name().startswith(path+'.'),obj.get_path_name()
 ref=obj.get_editor_property('CameraReference')
 before=ref.export_text()
 assert 'CameraAsset_SandboxCharacter' in before
 shutil.copy2(root/('Content/PlayerCharacters/BP_Sov'+name+'.uasset'),out/('BP_Sov'+name+'.before.uasset'))
 rows.append((bp,obj,ref,before))
report=[]
for bp,obj,ref,before in rows:
 ref.set_editor_property('CameraAsset',camera)
 obj.set_editor_property('CameraReference',ref)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
 report.append(dict(asset=bp.get_path_name(),before=before,after=ref.export_text()))
(out/'binding.json').write_text(json.dumps(report,indent=2))
unreal.log('SOV_SHOULDER_BINDING_SAVED_REQUIRES_GAMEPLAY_VALIDATION')
