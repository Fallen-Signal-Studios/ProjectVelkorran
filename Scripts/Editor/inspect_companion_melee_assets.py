"""Export actual weapon/attack Blueprint graphs and defaults without asset writes."""
import hashlib
import json
import os
from datetime import datetime
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / ('companion-melee-assets-' + datetime.now().strftime('%H%M%S-%f'))
out.mkdir(exist_ok=False)
report = dict(read_only=True, assets=[])
queue = [unreal.load_asset('/Game/Items/Weapons/WI_' + name) for name in ('Verity', 'Velkorran')]
seen = set()
while queue:
    asset = queue.pop(0)
    if not asset or asset.get_path_name() in seen:
        continue
    seen.add(asset.get_path_name())
    if not isinstance(asset, unreal.Blueprint):
        continue
    cls = asset.generated_class()
    cdo = unreal.get_default_object(cls)
    if isinstance(cdo, unreal.WeaponItem):
        visual = cdo.get_editor_property('weapon_visual_class')
        if visual:
            queue.append(unreal.load_asset(visual.get_path_name().split('.')[0]))
        for ability in cdo.get_editor_property('weapon_abilities'):
            queue.append(unreal.load_asset(ability.get_path_name().split('.')[0]))
    parent = unreal.AssetRegistryHelpers.get_tag_value(unreal.AssetRegistryHelpers.create_asset_data(asset), 'ParentClass')
    assert parent, 'Missing parent class for ' + asset.get_path_name()
    parent_path = str(parent).split("'")[1] if parent and "'" in str(parent) else str(parent or '')
    if parent_path.startswith('/') and not parent_path.startswith('/Script/'):
        queue.append(unreal.load_asset(parent_path.split('.')[0]))
    filename = out / (asset.get_name() + '.t3d')
    assert not filename.exists(), 'Preserve prior export'
    before = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(asset)
    task = unreal.AssetExportTask()
    for key, value in dict(object=asset, exporter=unreal.ObjectExporterT3D(), filename=str(filename),
                           automated=True, prompt=False, selected=False, replace_identical=False).items():
        task.set_editor_property(key, value)
    assert unreal.Exporter.run_asset_export_task(task), str(task.get_editor_property('errors'))
    assert before == unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(asset), 'Export changed Blueprint state'
    defaults_file = out / (asset.get_name() + '-defaults.t3d')
    assert not defaults_file.exists(), 'Preserve previous defaults export'
    task.set_editor_property('object', cdo)
    task.set_editor_property('filename', str(defaults_file))
    assert unreal.Exporter.run_asset_export_task(task), str(task.get_editor_property('errors'))
    assert before == unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(asset), 'Defaults export changed Blueprint state'
    props = {}
    for key in ('default_bot_attack_range', 'bot_attack_min_range', 'bot_attack_max_range', 'attack_definition'):
        try:
            props[key] = str(cdo.get_editor_property(key))
        except Exception:
            pass
    report['assets'].append(dict(asset=asset.get_path_name(), parent=parent_path,
        properties=props, export=str(filename), sha256=hashlib.sha256(filename.read_bytes()).hexdigest()))
(out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('COMPANION_MELEE_ASSET_EXPORT_COMPLETE')
