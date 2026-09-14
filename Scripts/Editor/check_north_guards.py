"""Visible parapets match retained barriers; isolated Pawn clearance controls."""
import json
from pathlib import Path
import unreal

def placements():
    return [(f'{side}_{i}',(x,-7900+400*i if i<14 else -2400,0),90) for side,x in [('West',-7288),('East',-6712)] for i in range(15)]

def check_guards(world,actors):
    labels={a.get_actor_label():a for a in actors}
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/ParapetClosingKit/north-guard-fit.json').read_text())
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for name,pos,yaw in placements():
        a=labels['KIT_North_ApproachGuard_'+name];c=a.static_mesh_component
        p=a.get_actor_location();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False)
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),pos))<.01
        assert all(abs(v-1)<.001 for v in (s.x,s.y,s.z)) and abs(a.get_actor_rotation().yaw-yaw)<.01
        assert max(abs(v-w) for v,w in zip((e.x,e.y,e.z),(11,100 if name.endswith('_14') else 200,65)))<.1 and abs(o.z-65)<.1
        assert c.static_mesh.get_name()==('SM_Aurelion_KIT_Parapet_2m' if name.endswith('_14') else 'SM_Aurelion_KIT_Parapet_4m')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert sm.get_convex_collision_count(c.static_mesh)==0 and sm.get_simple_collision_count(c.static_mesh)==0
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
    guards=[]
    for b in fit['baseline']:
        a=labels[b['actor']];c=a.static_mesh_component;guards.append(a)
        assert c.get_world_transform().export_text()==b['transform'] and c.static_mesh.get_path_name()==b['mesh']
        assert c.get_editor_property('visible')==b['visible'] and c.get_editor_property('hidden_in_game')==b['hidden_in_game']
        assert str(c.get_collision_enabled())==b['collision'] and str(c.get_collision_profile_name())==b['profile'] and a.get_actor_enable_collision()
    ignored=[a for a in actors if a not in guards];probes=[]
    for y in tuple(-7900+400*i for i in range(14))+(-2400,):
        for dx,z,expected in [(-400,93,True),(400,93,True),(-400,230,False),(400,230,False),(0,93,False)]:
            start=unreal.Vector(-7000,y,z);end=unreal.Vector(-7000+dx,y+100 if dx==0 else y,z)
            raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert bool(hit and hit.to_tuple()[0])==expected,(y,dx,z,expected)
            probes.append(dict(y=y,dx=dx,z=z,blocking=expected))
    return dict(placements=30,retained_guard_count=2,pawn_controls=probes,qualification='Isolated guard collision and visual fit; full bridge art and live traversal remain pending.')
