"""Export Z02 shell sources and measure retained floor, curve and approach surfaces."""
import json
import os
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()); labels={a.get_actor_label():a for a in actors}
shell_names=('Cube_2','Cube_3','Cube_4','Cube_5','Cube_6','Cube_7'); source_names=('Cube2','bridge2')+shell_names
assets=[]
for label in source_names:
    a=labels[label]; c=a.static_mesh_component; mesh=c.static_mesh
    task=unreal.AssetExportTask(); task.object=mesh; task.filename=str(out/(label+'.fbx')); task.automated=True; task.prompt=False
    task.replace_identical=False; task.exporter=unreal.StaticMeshExporterFBX(); assert unreal.Exporter.run_asset_export_task(task)
    origin,extent=a.get_actor_bounds(False)
    assets.append(dict(actor=label,mesh=mesh.get_path_name(),transform=a.get_actor_transform().export_text(),bounds_origin=origin.export_text(),bounds_extent=extent.export_text()))
def trace(start,end,selected,complex=True):
    ignored=[a for a in actors if a.get_actor_label() not in selected]
    raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*start),unreal.Vector(*end),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex,ignored,unreal.DrawDebugTrace.NONE,True)
    h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
    if h is None or not h.to_tuple()[0]:return None
    t=h.to_tuple(); p=t[5]
    return dict(position=[p.x,p.y,p.z],normal=t[6].export_text(),actor=t[9].get_actor_label())
floors=[]
for x in (-8520,-8400,-8000,-7600,-7200,-7000,-6800,-6400,-6000,-5600,-5510):
    for y in (-9890,-9800,-9400,-9000,-8600,-8200,-8110):
        floors.append(dict(x=x,y=y,complex=trace((x,y,100),(x,y,-60),('Cube2','Z02_Floor')),simple=trace((x,y,100),(x,y,-60),('Cube2','Z02_Floor'),False)))
curves=[]
for y in (-9700,-9400,-9000,-8600,-8200):
    for z in (25,100,200,300,400,500,600,700,800,900,950):
        for side,end_x in (('West',-9000),('East',-5000)):
            curves.append(dict(side=side,y=y,z=z,hit=trace((-7000,y,z),(end_x,y,z),shell_names)))
approach=[]
for y in range(-11900,-9799,100):
    approach.append(dict(y=y,complex=trace((-7000,y,650),(-7000,y,-100),('bridge2','Cube2')),simple=trace((-7000,y,650),(-7000,y,-100),('bridge2','Cube2'),False)))
(out/'z02-surfaces.json').write_text(json.dumps(dict(scope='Isolated geometry sampling; not live route qualification',assets=assets,floors=floors,curves=curves,approach=approach),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
