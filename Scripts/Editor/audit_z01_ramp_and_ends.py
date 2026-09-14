"""Read-only ramp/door source export and isolated ramp surface sampling."""
import json
import os
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
labels={a.get_actor_label():a for a in actors}; rows=[]
for label in ('ramp','aureliondoors'):
    a=labels[label]; c=a.static_mesh_component; mesh=c.static_mesh
    task=unreal.AssetExportTask(); task.object=mesh; task.filename=str(out/(label+'.fbx'))
    task.automated=True; task.prompt=False; task.replace_identical=False; task.exporter=unreal.StaticMeshExporterFBX()
    assert unreal.Exporter.run_asset_export_task(task)
    origin,extent=a.get_actor_bounds(False)
    rows.append(dict(label=label,mesh=mesh.get_path_name(),transform=a.get_actor_transform().export_text(),
        bounds_origin=origin.export_text(),bounds_extent=extent.export_text(),collision=str(c.get_collision_enabled()),
        profile=str(c.get_collision_profile_name()),materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())]))
ignored=[a for a in actors if a!=labels['ramp']]; samples=[]
for x in (-6360,-6280,-6200,-6120,-6040,-5960):
    for y in range(-16500,-12999,100):
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,650),unreal.Vector(x,y,-50),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignored,unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        if hit is None:
            samples.append(dict(x=x,y=y,hit=False,z=None,normal=None)); continue
        data=hit.to_tuple()
        samples.append(dict(x=x,y=y,hit=data[0],z=data[5].z if data[0] else None,normal=data[6].export_text() if data[0] else None))
(out/'ramp-and-ends.json').write_text(json.dumps(dict(assets=rows,ramp_samples=samples,scope='Isolated geometry queries; not live route traversal'),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
