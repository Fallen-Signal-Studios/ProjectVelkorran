"""Verify full relay floor coverage and the retained native walking surfaces."""
from pathlib import Path
import json,runpy,unreal

def check_z04_floors(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04FloorKit'
    old=json.loads((source/'floor-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text())
    labels={a.get_actor_label():a for a in actors};a=labels[old['actor']]
    assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    components=a.get_components_by_class(unreal.InstancedStaticMeshComponent);assert len(components)==5
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);actual=[]
    for c in components:
        m=c.static_mesh;spec=next(s for s in manifest['modules'] if s['asset']==m.get_name())
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        n=sm.get_nanite_settings(m);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        slots={str(s.material_slot_name):s.material_interface for s in m.get_editor_property('static_materials')}
        assert slots['M_Aurelion_IvoryStone'].get_path_name()=='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Z03FloorStone.M_AurelionKit_Z03FloorStone'
        b=m.get_bounds();extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.02
        expected={'Z04Paving':(387.5,380,7.5),'Z04Ascent':(600.4,1200,340),
                  'Z04Balcony':(1400.4,1100,40),'Z04LowCoverCap':(300,200,7.5),'Z04HighCoverCap':(300,300,7.5)}[m.get_name().replace('SM_Aurelion_KIT_','')]
        assert max(abs(2*extent[i]-expected[i]) for i in (0,1))<.02 and abs(2*extent[2]-expected[2])<.3
        for index in range(c.get_instance_count()):
            t=c.get_instance_transform(index,world_space=True);p=t.translation;r=t.rotation.rotator()
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
            actual.append((m.get_name(),round(p.x,3),round(p.y,3),round(p.z,3),round(r.yaw%360,3)))
    assert sorted(actual)==sorted((r['asset'],*[round(v,3) for v in r['location_cm']],r['yaw']%360) for r in manifest['placements']) and len(actual)==165
    # Independent dimensions and placements from native room, balcony and ramp datums.
    room=[r for r in actual if r[0].endswith('Z04Paving')];assert len(room)==160
    xs=sorted(set(r[1] for r in room));ys=sorted(set(r[2] for r in room))
    assert len(xs)==16 and len(ys)==10 and xs[0]-193.75==3900 and xs[-1]+193.75==10100 and ys[0]-190==-12900 and ys[-1]+190==-9100
    assert xs==[3900+(i+.5)*387.5 for i in range(16)] and ys==[-12900+(j+.5)*380 for j in range(10)]
    assert {(r[1],r[2],r[3],r[4]) for r in actual if r[0].endswith('Z04Ascent')}=={(9300,-11200,0,0),(7700,-9900,0,270)}
    assert [(r[1],r[2],r[3]) for r in actual if r[0].endswith('Z04Balcony')]==[(9000,-10050,300)]
    runpy.run_path(str(root/'Scripts/Editor/check_z04_ceiling.py'))['check_z04_ceiling'](actors)
    contacts=[]
    def contact(label,x,y,z):
        body=labels[label].get_component_by_class(unreal.StaticMeshComponent)
        hit=body.line_trace_component(unreal.Vector(x,y,z+20),unreal.Vector(x,y,z-60),False,False,False)
        assert hit is not None and abs(hit[0].z-z)<.02,(label,x,y,z,hit)
        contacts.append(dict(actor=label,x_cm=x,y_cm=y,height_cm=hit[0].z))
    for r in room:contact('Z04_Floor',r[1],r[2],0)
    for transverse in (-200,0,200):
        for offset in (-500,-300,-100,100,300,500):
            contact('Z04_SouthAscent_6x12_Rise3m',9300+transverse,-11200+offset,150+offset*.25)
            contact('Z04_WestDescent_12x6_Rise3m',7700+offset,-9900+transverse,150+offset*.25)
    for i in range(7):
        for j in range(6):contact('Z04_FlankBalcony_14x11_Top3m',8400+i*200,-10600+(j+.5)*1100/6,300)
    contact('Z04_LC_Balcony',9050,-10300,420);contact('Z04_HC_BalconyFoot',8350,-10800,220)
    assert len(contacts)==240 and len(actors)==3140
    return dict(room_bays=160,room_area_m2=2356,balcony_area_m2=154,ramps=2,cover_caps=2,
                replaced_vendor_instances=186,native_contacts=contacts,
                qualification='Saved geometry and native walking-plane fit only; live routes, complete art and packaged performance remain unqualified.')
