"""Read-only refuge, ramp and nearby shared-art survey before source authoring."""
import json, os, time, itertools
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==2832
def xyz(v):return [v.x,v.y,v.z]
physical=[]
for a in actors:
    if 'Refuge' not in a.get_actor_label():continue
    o,e=a.get_actor_bounds(False)
    physical.append(dict(actor=a.get_actor_label(),transform=a.get_actor_transform().export_text(),origin=xyz(o),extent=xyz(e),components=[dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game')) for c in a.get_components_by_class(unreal.StaticMeshComponent)]))
art=[]
for a in actors:
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m=c.static_mesh
        if not m:continue
        b=m.get_bounds();o=b.origin;e=b.box_extent
        corners=[unreal.Vector(o.x+x*e.x,o.y+y*e.y,o.z+z*e.z) for x,y,z in itertools.product((-1,1),repeat=3)]
        selected=[]
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);p=t.translation
            if not (750<=p.x<=1450 and 8300<=p.y<=10000 and -650<=p.z<=-300):continue
            points=[unreal.MathLibrary.transform_location(t,v) for v in corners]
            selected.append(dict(index=i,transform=t.export_text(),bounds=[[min(getattr(v,k) for v in points) for k in ('x','y','z')],[max(getattr(v,k) for v in points) for k in ('x','y','z')]]))
        if selected:art.append(dict(actor=a.get_actor_label(),component=c.get_name(),mesh=m.get_path_name(),count=c.get_instance_count(),nearby=selected,all_transforms=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]))
surfaces=[];targets=[a for a in actors if 'Refuge' in a.get_actor_label() and 'Guard' not in a.get_actor_label()];ignored=[a for a in actors if a not in targets]
for x in (850,1000,1100,1200,1350):
    for y in (8500,8600,8700,8800,8890,8910,9100,9400,9700,9890):
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,-350),unreal.Vector(x,y,-700),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        blocked=bool(h and h.to_tuple()[0]);t=h.to_tuple() if blocked else None
        surfaces.append(dict(x=x,y=y,blocked=blocked,z=t[5].z if t else None,normal=xyz(t[6]) if t else None,actor=t[9].get_actor_label() if t and t[9] else None))
(out/'refuge-survey.json').write_text(json.dumps(dict(status='read_only',physical=physical,nearby_art=art,surface_probes=surfaces,qualification='Nearby instances are candidates, not an approved removal set. Stopped-editor isolated surface measurements only.'),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('refuge-front',unreal.Vector(450,8050,-340),unreal.Rotator(pitch=-3,yaw=62),80)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('refuge-top',unreal.Vector(200,9700,-40),unreal.Rotator(pitch=-28,yaw=-15),85)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'refuge_survey_capture','exec'))
