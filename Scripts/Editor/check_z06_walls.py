"""Custom long-wall fit, retained instances and isolated physical wall controls."""
import json
from pathlib import Path
import unreal

def check_z06_walls(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir())
    fit=json.loads((root/'Art/Source/Aurelion/Z06WallFit/wall-fit.json').read_text());sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);placed=[]
    for side,sign,yaw in [('West',-1,-90),('East',1,90)]:
        for kind,suffix,stations,face,width in [('Wall','WallPlain_4x7',fit['wall_centers_y'],1674,400),('Pier','Pier_1x7',fit['pier_centers_y'],1574,120)]:
            for i,y in enumerate(stations):
                a=labels[f'KIT_Z06_Side_{side}_{kind}_{i:02}'];p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False);c=a.static_mesh_component;m=c.static_mesh
                assert abs(p.y-y)<.01 and abs(o.z-e.z+600)<.01 and 695<e.z*2<701
                assert abs(o.x-sign*e.x-sign*face)<.01 and abs(e.y*2-width)<.01
                assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and abs(r.yaw-yaw)<.001 and abs(r.pitch)+abs(r.roll)<.001
                assert m.get_name()=='SM_Aurelion_KIT_'+suffix and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
                assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
                assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
                placed.append(a.get_actor_label())
    row=fit['wall'];c=labels[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    expected=fit['retained_transforms'];end_count=sum(a.get_actor_label().startswith('KIT_Z06_End_') for a in actors)
    assert end_count in (0,18)
    if end_count:
        end_fit=json.loads((root/'Art/Source/Aurelion/Z06EndwallFit/endwall-fit.json').read_text())
        assert sorted(expected)==sorted([r['transform'] for r in end_fit['selected']]+end_fit['retained_transforms'])
        expected=end_fit['retained_transforms']
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(expected)
    retained_wall_count=c.get_instance_count()
    row=fit['columns'];c=labels[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    physical=[]
    for row in fit['physical']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
        physical.append(a)
    ignored=[a for a in actors if a not in physical];probes=[]
    for sign in (-1,1):
        for stations,face in [(fit['wall_centers_y'],1675),(fit['pier_centers_y'],1575)]:
            for y in stations:
                for z in (-450,-250,-50):
                    raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(0,y,z),unreal.Vector(sign*1800,y,z),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
                    hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
                    assert hit and hit.to_tuple()[0];t=hit.to_tuple();assert t[9] in physical and abs(t[5].x-sign*face)<.01,(sign,y,z,t[5])
                    probes.append(dict(x=t[5].x,y=y,z=z,actor=t[9].get_actor_label()))
    return dict(placements=placed,retained_wall_instances=retained_wall_count,removed_side_wall_instances=112,retained_hidden_column_instances=8,physical_probes=probes,qualification='Isolated editor wall/column controls and visual fit; live movement, wall-running, combat and performance remain unqualified.')
