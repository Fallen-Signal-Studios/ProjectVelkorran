"""Fitted approach guard geometry, corner overlap and physical route controls."""
import json,itertools
from pathlib import Path
import unreal

def get_fit():return json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/AtriumApproachKit/guard-fit.json').read_text())

def route_controls(world):
    rows=[]
    for start,end,axis in [((-6200,-2000,104.134024),(-5000,-2000,104.134024),1),((-5000,-2000,104.134024),(-5000,0,104.134024),0),((-5000,0,104.134024),(-3550,0,104.134024),1)]:
        for offset in (-170,0,170):
            a=list(start);b=list(end);a[axis]+=offset;b[axis]+=offset
            hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(*a),unreal.Vector(*b),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
            blocker=hit.to_tuple()[9].get_actor_label() if hit and hit.to_tuple()[0] else None
            assert blocker is None,('Approach lane blocked',a,b,blocker)
            rows.append(dict(start=a,end=b,clear=True))
    return rows

def check_guards(world,actors):
    labels={a.get_actor_label():a for a in actors};fit=get_fit();sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);boxes=[];probes=[]
    for spec in fit['placements']:
        a=labels['KIT_Atrium_ApproachGuard_'+spec['name']];c=a.static_mesh_component;p=a.get_actor_location();scale=a.get_actor_scale3d()
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),spec['position']))<.01
        assert max(abs(v-1) for v in (scale.x,scale.y,scale.z))<.001 and abs(a.get_actor_rotation().yaw-spec['yaw'])<.01
        assert c.static_mesh.get_name()==spec['mesh'] and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert sm.get_convex_collision_count(c.static_mesh)==0 and sm.get_simple_collision_count(c.static_mesh)==0
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
        o,e=a.get_actor_bounds(False);wanted=(spec['length_cm']/2,11,65) if spec['yaw']==0 else (11,spec['length_cm']/2,65)
        assert max(abs(v-w) for v,w in zip((e.x,e.y,e.z),wanted))<.1
        boxes.append(([o.x-e.x,o.y-e.y,o.z-e.z],[o.x+e.x,o.y+e.y,o.z+e.z],spec['name']))
    for a,b in itertools.combinations(boxes,2):
        assert not all(min(a[1][k],b[1][k])-max(a[0][k],b[0][k])>.02 for k in range(3)),('Overlapping parapets',a[2],b[2])
    for b in fit['baseline']:
        a=labels[b['actor']];c=a.static_mesh_component
        assert c.get_world_transform().export_text()==b['transform'] and c.static_mesh.get_path_name()==b['mesh']
        assert c.get_editor_property('visible')==b['visible'] and c.get_editor_property('hidden_in_game')==b['hidden_in_game']
        assert str(c.get_collision_enabled())==b['collision'] and str(c.get_collision_profile_name())==b['profile'] and a.get_actor_enable_collision()
        ignored=[other for other in actors if other!=a];o=a.get_actor_location();normal=1-b['axis']
        for z,expected in [(93,True),(230,False)]:
            start=[o.x,o.y,z];end=start[:];start[normal]-=150;end[normal]+=150
            hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(*start),unreal.Vector(*end),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
            assert bool(hit and hit.to_tuple()[0])==expected,(b['actor'],z)
            probes.append(dict(guard=b['actor'],z=z,blocking=expected))
    return dict(placements=len(boxes),guard_controls=probes,route_controls=route_controls(world),qualification='Geometry and capsule controls; live navigation, combat and GPU acceptance separate.')
