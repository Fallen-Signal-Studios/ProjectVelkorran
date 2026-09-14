"""Read-only current slab art ownership and surrounding passage measurements."""
import json,os,time,math,itertools
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==2831
slab=by_label['Z06_Fallen_Plate'];p=slab.get_actor_location();s=slab.get_actor_scale3d();yaw=math.radians(slab.get_actor_rotation().yaw)
def xyz(v):return [v.x,v.y,v.z]
def local(v):
    d=v-p;return [d.x*math.cos(yaw)+d.y*math.sin(yaw),-d.x*math.sin(yaw)+d.y*math.cos(yaw),d.z]
limits=[abs(v)*50 for v in xyz(s)];rows=[]
for a in actors:
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m=c.static_mesh
        if not m:continue
        b=m.get_bounds();o=b.origin;e=b.box_extent;corners=[unreal.Vector(o.x+x*e.x,o.y+y*e.y,o.z+z*e.z) for x,y,z in itertools.product((-1,1),repeat=3)];selected=[]
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);points=[local(unreal.MathLibrary.transform_location(t,v)) for v in corners]
            if all(abs(v[k])<=limits[k]+.1 for v in points for k in range(3)):
                selected.append(dict(index=i,transform=t.export_text(),local_bounds=[[min(v[k] for v in points) for k in range(3)],[max(v[k] for v in points) for k in range(3)]]))
        if selected:rows.append(dict(actor=a.get_actor_label(),component=c.get_name(),mesh=m.get_path_name(),count=c.get_instance_count(),selected=selected,all_transforms=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]))
probes=[]
for x in (-1100,-900,-700,700,900,1100):
    raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,7900,-500),unreal.Vector(x,9300,-500),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
    h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
    blocked=bool(h and h.to_tuple()[0]);probes.append(dict(x=x,start_y=7900,end_y=9300,z=-500,blocked=blocked,actor=h.to_tuple()[9].get_actor_label() if blocked and h.to_tuple()[9] else None))
c=slab.static_mesh_component
(out/'slab-survey.json').write_text(json.dumps(dict(status='read_only',physical=dict(actor=slab.get_actor_label(),transform=slab.get_actor_transform().export_text(),mesh=c.static_mesh.get_path_name(),local_half_extent_cm=limits,collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name())),art=rows,passage_probes=probes,qualification='Stopped-editor ownership and whole-world capsule measurements; no live combat or navigation acceptance'),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('slab-front',unreal.Vector(-750,7550,-300),unreal.Rotator(pitch=-5,yaw=55),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('slab-top',unreal.Vector(200,9700,-40),unreal.Rotator(pitch=-30,yaw=-100),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'slab_survey_capture','exec'))
