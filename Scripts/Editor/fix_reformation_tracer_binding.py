"""Repair two reviewed reader bindings in the tracked shared weapon effect."""
from pathlib import Path
import hashlib,json,os,shutil,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);root=Path(unreal.Paths.project_dir())
path='/NarrativePro/Pro/Core/VFX/Lyra/Effects/Particles/Weapons/NS_WeaponFire_Tracer_Reformation'
asset=unreal.load_asset(path)
assert asset and unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset)
disk=root/'Plugins/Narrativeed3f9374a6eV6/Content/Pro/Core/VFX/Lyra/Effects/Particles/Weapons/NS_WeaponFire_Tracer_Reformation.uasset'
assert disk.is_file()
before=unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset).export_text()
readers=[unreal.load_object(None,asset.get_path_name()+':'+suffix) for suffix in (
    'SystemUpdateScript.NiagaraDataInterfaceParticleRead_3',
    'Sparks2_1.NiagaraScriptSource_0.NiagaraGraph_0.NiagaraNodeInput_1.SpawnParticlesFromOtherEmitter_Attribute_Reader')]
assert all(readers)
assert all(str(r.get_editor_property('EmitterBinding').get_editor_property('EmitterName'))=='ref' for r in readers)
shutil.copy2(disk,out/'NS_WeaponFire_Tracer_Reformation.before.uasset')
maps=[root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes=[hashlib.sha256(p.read_bytes()).hexdigest() for p in maps]
asset.modify()
for reader in readers:
    reader.modify();binding=reader.get_editor_property('EmitterBinding')
    binding.set_editor_property('EmitterName','Tracer');reader.set_editor_property('EmitterBinding',binding)
assert unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset)
assert unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset).export_text()==before
assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
assert [hashlib.sha256(p.read_bytes()).hexdigest() for p in maps]==hashes
(out/'tracer-binding-save.json').write_text(json.dumps(dict(status='saved_requires_fresh_readback',
    asset=path,readers=[r.get_path_name() for r in readers],
    bindings=[r.get_editor_property('EmitterBinding').export_text() for r in readers],
    emitter_renderer_and_user_parameters_unchanged=True,maps_unchanged=True,
    saved_sha256=hashlib.sha256(disk.read_bytes()).hexdigest()),indent=2))
