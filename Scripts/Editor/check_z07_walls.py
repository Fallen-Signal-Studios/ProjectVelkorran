"""Fitted complete gallery wall perimeter and preserved native collision."""
import json,runpy
from pathlib import Path
import unreal
def check_z07_walls(world,actors):
    root=Path(unreal.Paths.project_dir());labels={a.get_actor_label():a for a in actors};fit=json.loads((root/'Art/Source/Aurelion/Z07WallKit/wall-fit.json').read_text());sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for row in fit['placements']:
        a=labels[row['label']];c=a.static_mesh_component;m=c.static_mesh;p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False)
        assert m.get_name()=='SM_Aurelion_KIT_'+row['suffix']
        assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and abs(r.yaw-row['yaw'])<.001 and abs(r.pitch)+abs(r.roll)<.001
        assert abs(p.z-row['z'])<.01 and abs(o.z-e.z-row['z'])<.01 and abs(2*e.z-row['height'])<.01
        across='y' if row['axis']=='x' else 'x'
        assert abs(getattr(p,across)-row['along'])<.01 and abs(2*getattr(e,across)-row['width'])<.01
        face=getattr(o,row['axis'])-row['sign']*getattr(e,row['axis']);assert abs(face-row['face'])<.01,(row['label'],face)
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
        assert sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
    for row in fit['retired']:
        a=labels[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    probes=[]
    for row in fit['physical']:
        a=labels[row['actor']];c=a.static_mesh_component;assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert a.get_actor_enable_collision()==row['actor_collision'] and str(c.get_collision_enabled())==row['collision']
        o,e=a.get_actor_bounds(False);axis='x' if 'EW' in row['actor'] or 'Rib' in row['actor'] else 'y'
        ignored=[x for x in actors if x!=a]
        for z in (o.z-e.z*.7,o.z,o.z+e.z*.7):
            start=unreal.Vector(o.x,o.y,z);end=unreal.Vector(o.x,o.y,z);setattr(start,axis,getattr(o,axis)-getattr(e,axis)-100);setattr(end,axis,getattr(o,axis)+getattr(e,axis)+100)
            raw=unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next((v for v in raw if isinstance(v,unreal.HitResult)),None) if isinstance(raw,tuple) else raw
            assert hit and hit.to_tuple()[0] and hit.to_tuple()[9]==a
            probes.append(dict(actor=row['actor'],z=z))
    doors=runpy.run_path(str(root/'Scripts/Editor/check_z07_ceiling.py'))['check_z07_ceiling'](world,actors)['doorway_capsules']
    return dict(placements=len(fit['placements']),retained_wall_instances=48,retained_column_instances=2,physical_contacts=probes,doorway_capsules=doors,qualification='Stopped-editor fit and isolated native collision. Live routes, camera clearance, combat, final material quality and GPU performance remain unqualified.')
