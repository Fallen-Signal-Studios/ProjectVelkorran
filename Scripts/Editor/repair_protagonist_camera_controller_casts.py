"""Remap obsolete Narrative controller references in the owned protagonist graphs.

This repairs the failing camera getter entry cast, not camera-arbiter wiring.
No native source, mission maps, or vendor packages are saved.
"""
import json
import os
import shutil
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'CameraCastRepair'
out.mkdir(exist_ok=True)
root = Path(unreal.Paths.project_dir())
source = unreal.load_asset('/NarrativePro/Pro/Core/BP/Framework/BP_NarrativePlayerController')
replacement = unreal.load_asset('/Game/Framework/BP_SovPlayerController')
targets = [unreal.load_asset('/Game/PlayerCharacters/BP_Sov'+name) for name in ('Tarrik','Selene')]
assert source and replacement and all(targets)
assert unreal.MathLibrary.class_is_child_of(replacement.generated_class(), unreal.SovPlayerController.static_class())
author = unreal.SovBlueprintAuthoringLibrary
source_before = author.fingerprint_blueprint(source)
# The remapper requires its replacement class in the explicit writable set.
# Back it up too; save only the two repaired protagonists, not the controller.
for bp in [replacement]+targets:
    relative = bp.get_path_name().split('.')[0].replace('/Game/', 'Content/')+'.uasset'
    backup = out/(bp.get_name()+'.before.uasset')
    assert not backup.exists(), 'Preserve original backup: '+str(backup)
    shutil.copy2(root/relative, backup)
result = author.remap_project_blueprint_references([replacement]+targets, [source], [replacement])
(out/'compile-report.txt').write_text(result.report)
assert result.succeeded, result.report
assert author.fingerprint_blueprint(source) == source_before, 'Vendor Blueprint changed in memory'
for bp in targets:
    task = unreal.AssetExportTask()
    filename = out/(bp.get_name()+'.after.t3d')
    for key,value in dict(object=bp,exporter=unreal.ObjectExporterT3D(),filename=str(filename),
            automated=True,prompt=False,selected=False,replace_identical=True).items():
        task.set_editor_property(key,value)
    assert unreal.Exporter.run_asset_export_task(task)
    raw = filename.read_bytes()
    text = raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
    assert '/NarrativePro/Pro/Core/BP/Framework/BP_NarrativePlayerController.' not in text
    assert '/Game/Framework/BP_SovPlayerController.' in text
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
(out/'camera-cast-repair.json').write_text(json.dumps(dict(status='saved_requires_runtime_review',
    assets=[bp.get_path_name() for bp in targets], vendor_unchanged=True,
    scope=__doc__), indent=2))
