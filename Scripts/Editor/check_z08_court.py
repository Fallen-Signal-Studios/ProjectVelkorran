"""Flush court, exact retained markings and unchanged native floor controls."""
from pathlib import Path
import json
import unreal
def check_z08_court(world,actors,nanite_enabled=True):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08CourtKit/court-fit.json').read_text());labels={a.get_actor_label():a for a in actors}
    a=labels['KIT_Z08_IsolationCourt'];c=a.static_mesh_component;m=c.static_mesh
    assert m.get_name()=='SM_Aurelion_KIT_Z08IsolationCourt'
    assert (a.get_actor_location()-unreal.Vector(*fit['location'])).length()<.001
    r=a.get_actor_rotation()
    assert (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.yaw)+abs(r.roll)<.001
    o,e=a.get_actor_bounds(False);assert abs(2*e.x-2200)<.01 and abs(2*e.y-1600)<.01 and abs(o.z+e.z+1199.8)<.01
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')==nanite_enabled and sm.get_convex_collision_count(m)==0
    assert sm.get_nanite_settings(m).get_editor_property('position_precision')==6
    for row in fit['underlay']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    row=fit['old'];a=labels[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert a.get_actor_transform().export_text()==row['actor_transform'] and c.get_world_transform().export_text()==row['component_transform'] and c.static_mesh.get_path_name()==row['mesh']
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(fit['retained_transforms']) and c.get_instance_count()==5
    row=fit['court'];a=labels[row['actor']];c=a.static_mesh_component
    assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh'] and str(c.get_collision_enabled())==row['collision']
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    floor=labels['Z08_Floor'];ignored=[a for a in actors if a not in [floor,labels[fit['court']['actor']]]];probes=[]
    for x in range(-1000,1001,200):
        for y in range(20100,21501,200):
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,-1100),unreal.Vector(x,y,-1300),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert hit and hit.to_tuple()[0] and hit.to_tuple()[9]==floor and abs(hit.to_tuple()[5].z+1200)<.01
            probes.append([x,y,-1200])
    return dict(nanite_enabled=nanite_enabled,position_precision=6,retained_markings=5,retired_court_instances=72,hidden_underlay_tiles=20,floor_contacts=probes,qualification='Saved surface fit and isolated native collision; live encounters, final lighting/materials and performance remain unqualified.')
