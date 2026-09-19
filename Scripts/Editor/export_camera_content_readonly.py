"""Export protagonist inheritance and camera interface graphs for handoff review.

No graph mutation, compilation, or asset saving. Reference presence alone is not
proof that the resolved camera state reaches the rig or has only one owner.
"""
import json
import os
import re
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'CameraContent'
out.mkdir(exist_ok=True)
queue = ['/Game/PlayerCharacters/BP_SovTarrik', '/Game/PlayerCharacters/BP_SovSelene',
         '/NarrativePro/Pro/Core/Character/Biped/Camera/BPI_GameplayCamera']
seen = set()
report = dict(scope=__doc__, assets=[], findings=[])
while queue:
    path = queue.pop(0)
    if path in seen:
        continue
    seen.add(path)
    bp = unreal.load_asset(path)
    if not isinstance(bp, unreal.Blueprint):
        report['findings'].append('Not a loadable Blueprint: '+path)
        continue
    filename = str(out/(bp.get_name()+'.t3d'))
    task = unreal.AssetExportTask()
    for key, value in dict(object=bp, exporter=unreal.ObjectExporterT3D(), filename=filename,
            automated=True, prompt=False, selected=False, replace_identical=True).items():
        task.set_editor_property(key, value)
    assert unreal.Exporter.run_asset_export_task(task), path
    raw = Path(filename).read_bytes()
    exported = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    match = re.search(r'^   ParentClass="[^\n]*?\x27([^\x27]+)\x27"', exported, re.M)
    parent_path = match.group(1) if match else None
    report['assets'].append(dict(path=path, parent=parent_path, export=filename))
    if parent_path and not parent_path.startswith('/Script/'):
        queue.append(parent_path.split('.')[0])
(out/'camera-content.json').write_text(json.dumps(report, indent=2))
