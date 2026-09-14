"""Read-only inventory of non-Aurelion enemy grants and their authored defaults."""
import json, os
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
rows = {}
seen = set()
def export(asset):
    if asset.get_path_name() in seen:
        return
    seen.add(asset.get_path_name())
    task = unreal.AssetExportTask()
    for key, value in dict(object=asset, exporter=unreal.ObjectExporterT3D(),
            filename=str(out/(asset.get_name()+'.t3d')), automated=True, prompt=False,
            replace_identical=True).items():
        task.set_editor_property(key,value)
    unreal.Exporter.run_asset_export_task(task)
for data in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game', recursive=True):
    name = str(data.asset_name)
    if not name.startswith('NPC_') or not any(n in name for n in ('Hound','Handler','Enforcer','Drone','Linkbound','Weaver','WallRunner','Elite')):
        continue
    asset = data.get_asset()
    try:
        config = asset.get_editor_property('ability_configuration')
        grants = list(config.get_editor_property('default_abilities'))
        rows[asset.get_path_name()] = [g.get_path_name() for g in grants]
        export(asset)
        for grant in grants:
            export(unreal.get_default_object(grant))
    except Exception as exc:
        rows[asset.get_path_name()] = {'error':str(exc)}
(out/'all-enemy-casts.json').write_text(json.dumps(rows,indent=2))
