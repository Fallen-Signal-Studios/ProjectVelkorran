"""Source-owned bridge placement, physical surfaces and open arch controls."""
import json
from pathlib import Path
import unreal

def hit_result(raw):
    return next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw

def route_controls(world,actors):
    labels={a.get_actor_label():a for a in actors};rows=[]
    for name in ('bridge2','bridge3'):
        p=labels[name].get_actor_location()
        for x in (-180,0,180):
            start=unreal.Vector(p.x+x,p.y-900,p.z+104.133914);end=unreal.Vector(start.x,p.y+900,start.z)
            hit=hit_result(unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True))
            rows.append(dict(bridge=name,x=x,blocker=hit.to_tuple()[9].get_actor_label() if hit and hit.to_tuple()[0] else None))
    return rows

def check_bridges(world,actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/ApproachBridgeKit'
    fit=json.loads((source/'bridge-fit.json').read_text());specs={s['asset']:s for s in json.loads((source/'manifest.json').read_text())['modules']}
    labels={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);probes=[];arch_controls=[];pawn_controls=[]
    for b in fit['baseline']:
        old=labels[b['actor']];c=old.static_mesh_component
        assert c.get_world_transform().export_text()==b['transform'] and c.static_mesh.get_path_name()==b['mesh']
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        assert not old.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        for suffix in ('ApproachDeck','ApproachArchSupports'):
            a=labels['KIT_Z02_Bridge_'+b['actor']+'_'+suffix];c=a.static_mesh_component;s=a.get_actor_scale3d();p=a.get_actor_location();base=old.get_actor_location()
            assert (p-base).length()<.01 and all(abs(v-1)<.001 for v in (s.x,s.y,s.z))
            rotation=a.get_actor_rotation();assert max(abs(rotation.pitch),abs(rotation.yaw),abs(rotation.roll))<.01
            assert c.static_mesh.get_name()=='SM_Aurelion_KIT_'+suffix and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
            assert a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS and str(c.get_collision_profile_name())=='BlockAll'
            spec=specs[c.static_mesh.get_name()]
            assert sm.get_convex_collision_count(c.static_mesh)==spec['convex_hulls'] and sm.get_simple_collision_count(c.static_mesh)==0
            assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
            ignored=[other for other in actors if other!=a]
            if suffix=='ApproachDeck':
                for x,y,z in spec['bridge_surface_samples']:
                    start=unreal.Vector(p.x+x*100,p.y-y*100,p.z+500);end=unreal.Vector(start.x,start.y,p.z-100)
                    for complex_trace in (False,True):
                        hit=hit_result(unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex_trace,ignored,unreal.DrawDebugTrace.NONE,True))
                        assert hit and hit.to_tuple()[0],(a.get_actor_label(),x,y,complex_trace)
                        actual=hit.to_tuple()[5].z;expected=p.z+z*100
                        assert abs(actual-expected)<(.51 if complex_trace else .05),(x,y,actual,expected)
                        probes.append(dict(actor=a.get_actor_label(),x=x,y=y,complex=complex_trace,z=actual))
                top=min(row[2] for row in spec['bridge_surface_samples'])
                for y in (-900,0,900):
                    for dx,z,expected in ((800,93,True),(-800,93,True),(800,230,False),(-800,230,False),(0,93,False)):
                        start=unreal.Vector(p.x,p.y+y,p.z+top*100+z);end=unreal.Vector(p.x+dx,start.y+100 if dx==0 else start.y,start.z)
                        hit=hit_result(unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True))
                        assert bool(hit and hit.to_tuple()[0])==expected,(a.get_actor_label(),y,dx,z,expected)
                        pawn_controls.append(dict(actor=a.get_actor_label(),y=y,dx=dx,z=z,blocking=expected))
            else:
                for y,z,expected in spec['support_lateral_samples']:
                    start=unreal.Vector(p.x-1000,p.y-(y+.05)*100,p.z+z*100);end=unreal.Vector(p.x+1000,start.y,start.z)
                    for complex_trace in (False,True):
                        hit=hit_result(unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex_trace,ignored,unreal.DrawDebugTrace.NONE,True))
                        assert bool(hit and hit.to_tuple()[0])==expected,(y,z,complex_trace,expected)
                        arch_controls.append(dict(actor=a.get_actor_label(),y=y,z=z,complex=complex_trace,blocking=expected))
    return dict(surface_probes=probes,arch_controls=arch_controls,pawn_controls=pawn_controls,qualification='Isolated geometry/physics fit, not live traversal, navigation, combat or GPU acceptance.')
