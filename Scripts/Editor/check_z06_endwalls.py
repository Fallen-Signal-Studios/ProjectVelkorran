"""End-wall fit, retained climb art and isolated doorway clearance controls."""
import json
from pathlib import Path
import unreal

def aperture_checks(world,actors,fit,retired):
    labels={a.get_actor_label():a for a in actors};physical=[labels[r['actor']] for r in fit['physical']+fit['bands']];ignored=[a for a in actors if a not in physical];samples=[]
    def hit(raw):
        if raw is None:return None
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        return h if h and h.to_tuple()[0] else None
    for side,y in [('South',6000),('North',11600)]:
        for x in (-248,0,248):
            for z in (-507,-395,-275):
                h=hit(unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,y-400,z),unreal.Vector(x,y+400,z),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True))
                if not retired:
                    if h:assert h.to_tuple()[9] in [labels[r['actor']] for r in fit['bands']],(side,x,z,'Unexpected baseline obstruction')
                else:assert h is None,(side,x,z,'Portal obstructed')
                samples.append(dict(side=side,x=x,z=z,blocked=bool(h)))
        for x,z in [(-350,-375),(350,-375),(0,-50)]:
            h=hit(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y-100,z),unreal.Vector(x,y+100,z),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True))
            assert h and h.to_tuple()[9] in [labels[r['actor']] for r in fit['physical']]
    return samples

def check_z06_endwalls(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z06EndwallFit/endwall-fit.json').read_text());sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);placed=[]
    for side,face,sign,yaw in [('South',6026,1,0),('North',11574,-1,180)]:
        for row in fit['layout']:
            a=labels[f'KIT_Z06_End_{side}_{row["name"]}'];p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False);c=a.static_mesh_component;m=c.static_mesh
            assert abs(p.x-row['x'])<.01 and abs(o.z-e.z+600)<.01 and abs(o.y+sign*e.y-face)<.01 and abs(e.x*2-row['width'])<.01
            assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and abs((r.yaw-yaw+180)%360-180)<.01 and abs(r.pitch)+abs(r.roll)<.01
            assert m.get_name()=='SM_Aurelion_KIT_'+row['suffix'] and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
            assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
            for i,slot in enumerate(m.get_editor_property('static_materials')):
                if str(slot.get_editor_property('imported_material_slot_name'))=='M_Aurelion_IvoryStone':assert c.get_material(i).get_name()=='M_AurelionKit_PavingIvory'
            placed.append(a.get_actor_label())
    c=labels[fit['wall_actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==fit['retained_transforms']
    for row in fit['physical']+fit['bands']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        if row in fit['bands']:
            assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        else:assert a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    return dict(placements=placed,retained_climb_instances=1,removed_endwall_instances=52,aperture_capsules=aperture_checks(world,actors,fit,True),wall_lintel_controls=6,qualification='Isolated doorway collision controls; decorative band collision intentionally retired. Live mission traversal, wall-running and performance remain unqualified.')
