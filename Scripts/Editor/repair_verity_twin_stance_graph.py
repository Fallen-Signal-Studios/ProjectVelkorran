"""Apply the stance node to the persistent Overlay graph, then verify its export."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/VerityTwinBlades'
parent = unreal.load_asset(root + '/ABP_VerityTwinMeleeBase')
blend = unreal.load_asset(root + '/BS_VerityTwinStance')
overlay = unreal.load_asset('/Game/Characters/Animation/ABP_SovVerityOverlay')
for asset in (parent, blend, overlay):
    rel = asset.get_path_name().split('.')[0].replace('/Game/', 'Content/') + '.uasset'
    shutil.copy2(Path(unreal.Paths.project_dir()).resolve() / rel, out / (asset.get_name() + '-before.uasset'))
clips = [unreal.load_asset(root + '/SovVerity_Twinblades_' + n) for n in ('Idle', 'Walk_F', 'Run_F')]
result = unreal.SovBlueprintAuthoringLibrary.configure_verity_twin_locomotion(parent, blend, clips)
assert result.succeeded, result.report
unreal.BlueprintEditorLibrary.compile_blueprint(overlay)
task = unreal.AssetExportTask()
for k, v in dict(object=parent, exporter=unreal.ObjectExporterT3D(), filename=str(out/'stance-after.t3d'),
                 automated=True, prompt=False, selected=False, replace_identical=False).items():
    task.set_editor_property(k, v)
assert unreal.Exporter.run_asset_export_task(task)
raw = (out/'stance-after.t3d').read_bytes()
export = raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8-sig')
assert ':Overlay.' in export and 'VerityTwinStance' in export and 'BS_VerityTwinStance.BS_VerityTwinStance' in export
for asset in (parent, blend, overlay):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
(out/'stance-repair.json').write_text(json.dumps(dict(status='saved', compiler=str(result.report),
    persistent_graph_verified=True, live_gameplay_qualified=False), indent=2), encoding='utf-8')
