"""Read-only geometry export for the next Z02 windows, columns and furniture pass."""
import json
import os
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);labels={a.get_actor_label():a for a in subsystem.get_all_level_actors()}
names=[f'SM_KB3D_GAE_Window_C_Main{i}' for i in range(5,9)]+[f'SM_KB3D_GAE_Pilllar_A_Main{i}' for i in (3,4)]+[f'SM_KB3D_IRF_PropStoneTable_C_Main{i}' for i in range(2,8)]+['SM_KB3D_GOL_Trim_B_Main']
rows=[];exported={};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for name in names:
    a=labels[name];c=a.static_mesh_component;mesh=c.static_mesh;path=mesh.get_path_name();origin,extent=a.get_actor_bounds(False)
    if path not in exported:
        filename=mesh.get_name()+'.fbx';task=unreal.AssetExportTask();task.object=mesh;task.filename=str(out/filename);task.automated=True;task.prompt=False;task.replace_identical=False;task.exporter=unreal.StaticMeshExporterFBX()
        assert unreal.Exporter.run_asset_export_task(task);exported[path]=filename
    rows.append(dict(actor=name,mesh=path,source_export=exported[path],transform=a.get_actor_transform().export_text(),component_transform=c.get_world_transform().export_text(),
                     origin=[origin.x,origin.y,origin.z],extent=[extent.x,extent.y,extent.z],collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),
                     simple_primitives=sm.get_simple_collision_count(mesh),convex_hulls=sm.get_convex_collision_count(mesh),visible=c.get_editor_property('visible'),hidden_in_game=c.get_editor_property('hidden_in_game')))
(out/'z02-fittings-baseline.json').write_text(json.dumps(dict(scope='Geometry reference for custom replacements; no asset/map edits or gameplay acceptance',actors=rows),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
