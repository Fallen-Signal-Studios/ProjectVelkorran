"""Validate refuge support volume and unchanged ramp barrier geometry."""
import json
from pathlib import Path
import unreal

def check_z06_refuge_finish(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir());sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    plinth=labels['KIT_Z06_RefugeFinish_Plinth'];o,e=plinth.get_actor_bounds(False)
    assert (o-unreal.Vector(1100,9400,-570)).length()<.01 and (e-unreal.Vector(300,500,30)).length()<.01
    for suffix in ('Plinth','GuardWest','GuardEast'):
        a=labels['KIT_Z06_RefugeFinish_'+suffix];c=a.static_mesh_component;m=c.static_mesh;s=a.get_actor_scale3d();physical=suffix=='Plinth'
        assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001
        assert m.get_name()=='SM_Aurelion_KIT_Z06Refuge'+('Plinth' if physical else 'Guard')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and a.get_actor_enable_collision()==physical
        assert c.get_collision_enabled()==(unreal.CollisionEnabled.QUERY_AND_PHYSICS if physical else unreal.CollisionEnabled.NO_COLLISION)
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_convex_collision_count(m)==int(physical) and sm.get_simple_collision_count(m)==0
        if not physical:
            old=labels['Z06_Refuge_Ramp_Guard_'+('1' if suffix=='GuardWest' else '-1')];oo,ee=old.get_actor_bounds(False);o,e=a.get_actor_bounds(False)
            assert (o-oo).length()<.02 and (e-ee).length()<.02
            assert (a.get_actor_location()-old.get_actor_location()).length()<.01
            r=a.get_actor_rotation();rr=old.get_actor_rotation();assert max(abs(getattr(r,k)-getattr(rr,k)) for k in ('pitch','yaw','roll'))<.001
    fit=json.loads((root/'Art/Source/Aurelion/Z06RefugeKit/refuge-baseline.json').read_text());row=next(r for r in fit['nearby_art'] if r['count']==40);c=labels[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(row['all_transforms'][4:])
    ignored=[a for a in actors if a!=plinth];probes=[]
    for x in (850,1100,1350):
        for y in (8950,9400,9850):
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,-400),unreal.Vector(x,y,-650),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert h and h.to_tuple()[0] and h.to_tuple()[9]==plinth and abs(h.to_tuple()[5].z+540)<.01;probes.append([x,y,h.to_tuple()[5].z])
    # Capsules follow the existing ramp's 1:4 rise and continue onto the landing.
    # Isolate the architectural assembly; the encounter gate is intentionally excluded.
    route=[labels[n] for n in ('Z06_Refuge','Z06_Refuge_Ramp','Z06_Refuge_Ramp_Guard_1','Z06_Refuge_Ramp_Guard_-1')]+[plinth]
    ignored=[a for a in actors if a not in route];lanes=[]
    for x in (975,1100,1225):
        for y0,z0,y1,z1 in ((8520,-595,8890,-502.5),(8890,-502.5,9150,-500)):
            raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,y0,z0+95),unreal.Vector(x,y1,z1+95),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
            h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert not (h and h.to_tuple()[0]),(x,y0,h);lanes.append([x,y0,y1])
    return dict(placements=3,retained_railing_instances=36,plinth_top_probes=probes,ramp_capsule_lanes=lanes,qualification='New solid plinth under existing landing; isolated architectural clearance only. Rescue gate, live navigation, combat and performance remain unqualified.')
