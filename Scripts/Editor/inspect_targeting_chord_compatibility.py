"""Read-only wheel/controller graph and live input inspection for handoff task 5."""
import json
import os
import re
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'TargetingChordInspection'
out.mkdir(exist_ok=False)
queue = ['/Game/UI/Narrative/Menus/RadialMenus/WM_WeaponWheel_LR', '/Game/Framework/BP_SovPlayerController']
seen = set()
report = dict(assets=[], mappings=[])
while queue:
    path = queue.pop(0)
    if path in seen:
        continue
    seen.add(path)
    bp = unreal.load_asset(path)
    if not isinstance(bp, unreal.Blueprint):
        report['assets'].append(dict(path=path, missing=True))
        continue
    filename = out / (bp.get_name() + '.t3d')
    task = unreal.AssetExportTask()
    for key, value in dict(object=bp, exporter=unreal.ObjectExporterT3D(), filename=str(filename), automated=True,
                           prompt=False, selected=False, replace_identical=True).items():
        task.set_editor_property(key, value)
    assert unreal.Exporter.run_asset_export_task(task)
    raw = filename.read_bytes()
    exported = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    parent = re.search(r'^   ParentClass="[^\n]*?\x27([^\x27]+)\x27"', exported, re.M)
    parent = parent.group(1) if parent else None
    report['assets'].append(dict(path=path, parent=parent))
    if parent and not parent.startswith('/Script/'):
        queue.append(parent.split('.')[0])
context = unreal.load_asset('/Game/Input/IMC_Combat')
for row in context.get_editor_property('default_key_mappings').get_editor_property('mappings'):
    report['mappings'].append(dict(action=row.action.get_name(), key=str(row.key.get_editor_property('key_name')),
        triggers=[t.get_class().get_path_name() for t in row.triggers]))
(out / 'inspection.json').write_text(json.dumps(report, indent=2))

# Test an editor-only construction path without modifying the real context.
try:
    action = unreal.load_asset('/Game/Input/IA_WeaponWheel')
    trigger = unreal.new_object(unreal.InputTriggerChordAction)
    trigger.set_editor_property('chord_action', action)
    scratch = unreal.new_object(unreal.InputMappingContext)
    renamed = trigger.rename('ChordConstructionProbe', scratch)
    report['transient_trigger_probe'] = dict(succeeded=bool(renamed) and trigger.get_editor_property('chord_action') == action,
        outer=trigger.get_outer().get_path_name())
except Exception as exc:
    report['transient_trigger_probe'] = dict(succeeded=False, error=str(exc))
(out / 'inspection.json').write_text(json.dumps(report, indent=2))
