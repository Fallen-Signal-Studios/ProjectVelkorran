"""Fresh vendor cargo baseline, with native cover ownership evidence."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
census=json.loads((root/'Saved/Validation/Aurelion/RemainingEnvironmentAudit-20260914-193105-57b30f38/remaining-environment.json').read_text())
row=next(r for r in census['components'] if r['actor']=='Aurelion_Art_M12_Z04_43_a118a7')
a=next(a for a in actors if a.get_actor_label()==row['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert a.get_path_name()==row['path'] and c.static_mesh.get_path_name()==row['mesh']
row['all_instance_transforms']=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
assert row['all_instance_transforms']==[r['transform'] for r in row['instances']] and c.get_instance_count()==13
room=json.loads((root/'Art/Source/Aurelion/Z04WallKit/room-baseline.json').read_text());covers=[]
for before in room['named_actors']:
    if not before['actor'].startswith(('Z04_LC_','Z04_HC_')):continue
    cover=next(a for a in actors if a.get_actor_label()==before['actor']);body=cover.get_component_by_class(unreal.StaticMeshComponent)
    assert cover.get_actor_transform().export_text()==before['transform']
    origin,extent,_=unreal.SystemLibrary.get_component_bounds(body);probes=[]
    for axis in (0,1):
        start=[origin.x,origin.y,origin.z];end=start.copy();e=[extent.x,extent.y,extent.z]
        start[axis]-=e[axis]+10;end[axis]+=e[axis]+10
        hit=body.line_trace_component(unreal.Vector(*start),unreal.Vector(*end),False,False,False);assert hit
        probes.append([hit[0].x,hit[0].y,hit[0].z])
    covers.append(dict(actor=before['actor'],path=cover.get_path_name(),transform=before['transform'],component=body.get_path_name(),
        origin=[origin.x,origin.y,origin.z],extent=[extent.x,extent.y,extent.z],profile=str(body.get_collision_profile_name()),collision=str(body.get_collision_enabled()),contacts=probes))
assert len(covers)==9 and len(actors)==3140
row['native_covers']=covers
(out/'cargo-baseline.json').write_text(json.dumps(row,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
