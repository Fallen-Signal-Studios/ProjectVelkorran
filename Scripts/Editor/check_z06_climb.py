"""Exact replacement instance, unchanged traversal bodies, and isolated contacts."""
from pathlib import Path
import json,itertools
import unreal

def check_climb_instance(c,previous):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z06ClimbKit/climb-baseline.json').read_text())
    assert previous==fit['art']['all_instance_transforms']
    assert c.get_instance_count()==1 and c.static_mesh.get_name()=='SM_Aurelion_KIT_Z06ClimbPanel'
    t=c.get_instance_transform(0,world_space=True);r=t.rotation.rotator()
    assert (t.translation-unreal.Vector(-1384,9430,-600)).length()<.01
    assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.yaw-90)+abs(r.pitch)+abs(r.roll)<.001
    b=c.static_mesh.get_bounds()
    points=[unreal.MathLibrary.transform_location(t,b.origin+unreal.Vector(x*b.box_extent.x,y*b.box_extent.y,z*b.box_extent.z)) for x,y,z in itertools.product((-1,1),repeat=3)]
    bounds=[[fn(getattr(p,k) for p in points) for k in ('x','y','z')] for fn in (min,max)]
    assert max(abs(v-w) for actual,expected in zip(bounds,fit['art']['instances'][0]['bounds']) for v,w in zip(actual,expected))<.02
    return [t.export_text()]

def check_z06_climb(world,actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z06ClimbKit/climb-baseline.json').read_text());labels={a.get_actor_label():a for a in actors}
    art=fit['art'];a=labels[art['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    check_climb_instance(c,art['all_instance_transforms'])
    assert a.get_actor_transform().export_text()==art['actor_transform'] and c.get_world_transform().export_text()==art['component_transform']
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);m=c.static_mesh
    assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_simple_collision_count(m)==0
    for row in fit['physical']:
        a=labels[row['actor']];pc=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and pc.static_mesh.get_path_name()==row['mesh'] and a.get_actor_enable_collision()
        assert str(pc.get_collision_enabled())==row['collision'] and str(pc.get_collision_profile_name())==row['profile']
        assert pc.get_editor_property('visible')==row['visible'] and pc.get_editor_property('hidden_in_game')==row['hidden']
    contacts=[]
    def probe(label,start,end,axis,value):
        a=labels[label];ignored=[other for other in actors if other!=a]
        raw=unreal.SystemLibrary.line_trace_single_by_profile(world,unreal.Vector(*start),unreal.Vector(*end),'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert h and h.to_tuple()[0] and h.to_tuple()[9]==a
        point=h.to_tuple()[5];assert abs(getattr(point,axis)-value)<.01
        contacts.append(dict(actor=label,point=[point.x,point.y,point.z]))
    for y in (9250,9430,9610):
        for z in (-550,-450,-350):probe('Aurelion_E3_ClimbWall',(-1200,y,z),(-1450,y,z),'x',-1374)
    for y in (9520,9580,9640):
        for x in (-1330,-1270):probe('Aurelion_E3_FlankLanding',(x,y,-150),(x,y,-400),'z',-300)
    return dict(instances=1,physical_contacts=contacts,qualification='Editor bounds and original climb/landing collision contacts; live mantling, animation and final art remain unqualified.')
