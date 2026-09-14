"""Export the existing enclosure for an owned upper-shell derivative; no asset/map edits."""
import json
import os
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
matches=[a for a in actors if a.get_actor_label()=='aurelionwalls']
assert len(matches)==1
a=matches[0]; mesh=a.static_mesh_component.static_mesh
task=unreal.AssetExportTask(); task.object=mesh; task.filename=str(out/'Z01_OriginalEnclosure.fbx')
task.automated=True; task.prompt=False; task.replace_identical=False
task.exporter=unreal.StaticMeshExporterFBX()
assert unreal.Exporter.run_asset_export_task(task)
(out/'enclosure-export.json').write_text(json.dumps(dict(asset=mesh.get_path_name(),actor_transform=a.get_actor_transform().export_text(),
    materials=[a.static_mesh_component.get_material(i).get_path_name() for i in range(a.static_mesh_component.get_num_materials())]),indent=2))
