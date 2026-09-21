"""Read-only emitter/reader evidence for the live Reformation weapon tracer."""
from pathlib import Path
import json, os, unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
path='/NarrativePro/Pro/Core/VFX/Lyra/Effects/Particles/Weapons/NS_WeaponFire_Tracer_Reformation'
asset=unreal.load_asset(path)
assert asset
assert unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset)
info=unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset)
task=unreal.AssetExportTask()
for key,value in dict(object=asset,exporter=unreal.ObjectExporterT3D(),filename=str(out/'tracer.t3d'),
                      automated=True,prompt=False,selected=False,replace_identical=False).items():
    task.set_editor_property(key,value)
assert unreal.Exporter.run_asset_export_task(task)
(out/'tracer-inspection.json').write_text(json.dumps(dict(asset=path,readback=info.export_text(),
    emitters=[dict(name=str(e.name),enabled=e.enabled,gpu=e.gpu) for e in info.emitters],
    referencers=[str(n) for n in unreal.AssetRegistryHelpers.get_asset_registry().get_referencers(path,unreal.AssetRegistryDependencyOptions())],
    assets_saved=[]),indent=2))
