"""Verify the complete nine-object rib replacement against the original geometry."""
from pathlib import Path
import json,runpy,unreal

def check_z03_ribs(actors):
    root=Path(unreal.Paths.project_dir()); source=root/'Art/Source/Aurelion/Z03RibKit'
    old=json.loads((source/'rib-baseline.json').read_text()); manifest=json.loads((source/'manifest.json').read_text())
    a=next(a for a in actors if a.get_actor_label()==old['actor'])
    assert a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    components=list(a.get_components_by_class(unreal.InstancedStaticMeshComponent)); assert len(components)==2
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py')); sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    actual=[]; bounds_by_pose={}
    for c in components:
        m=c.static_mesh; spec=next(s for s in manifest['modules'] if s['asset']==m.get_name())
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        n=sm.get_nanite_settings(m); assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        b=m.get_bounds(); origin=[b.origin.x,b.origin.y,b.origin.z]; extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.02
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);r=t.rotation.rotator();p=t.translation
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
            pose=(round(p.x,2),round(p.y,2),round(p.z,2))
            actual.append((m.get_name(),*pose,round(r.yaw%360,2)))
            bounds_by_pose[pose]=geo['bounds'](geo['corners'](t,origin,extent))
    assert sorted(actual)==sorted((r['asset'],*r['location_cm'],r['yaw']%360) for r in manifest['placements'])
    assert len(actual)==9 and len(bounds_by_pose)==9
    # Independently unite the original six pier envelopes and three 15-piece cover envelopes.
    groups=[[i] for i in range(6)]+[list(range(start,start+15)) for start in (6,21,36)]
    for indices in groups:
        target=[[fn(old['instances'][i]['bounds'][side][axis] for i in indices) for axis in range(3)] for side,fn in ((0,min),(1,max))]
        pose=tuple(round((target[0][i]+target[1][i])/2,2) for i in range(2))+(0.,)
        fitted=bounds_by_pose[pose]
        assert max(abs(v-w) for x,y in zip(fitted,target) for v,w in zip(x,y))<.02
    runpy.run_path(str(root/'Scripts/Editor/check_z03_ceiling.py'))['check_z03_ceiling'](actors)
    contacts=[]
    for label in ('Z03_Frozen_Rib_12','Z03_Frozen_Rib_24','Z03_Frozen_Rib_35'):
        native=next(a for a in actors if a.get_actor_label()==label);body=native.get_component_by_class(unreal.StaticMeshComponent)
        p=native.get_actor_location()
        for axis,half in ((0,100),(1,200)):
            for height in (80,220):
                start=[p.x,p.y,height];end=start.copy();start[axis]-=half+20;end[axis]+=half+20
                hit=body.line_trace_component(unreal.Vector(*start),unreal.Vector(*end),False,False,False)
                assert hit is not None
                contact=[hit[0].x,hit[0].y,hit[0].z]
                assert abs(contact[axis]-(start[axis]+20))<.02
                contacts.append(dict(actor=label,axis=axis,height_cm=height,contact_cm=contact))
    assert len(actors)==3140
    return dict(piers=6,cover_assemblies=3,retired_vendor_instances=51,original_cover_contacts=contacts,
                qualification='Full original visual envelopes and native collision retained; live scanner concealment, final art and Chaos breakage unqualified.')
