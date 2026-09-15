"""Relay wayfinding presentation and native route checks."""
from pathlib import Path
import json,runpy,unreal
def route_placements():
    rows=[([7200,-12750+100*i,-.1],0) for i in range(9)]
    rows[-1][0][1]=-11960
    rows += [([x,-11900,-.1],90) for x in (7150,7050,6950)]
    tail=[([6900,-11850+100*i,-.1],0) for i in range(27)];tail[0][0][1]=-11840
    return rows+tail
def check_z04_wayfinding(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04WayfindingKit';labels={a.get_actor_label():a for a in actors}
    fit=json.loads((source/'presentation-baseline.json').read_text());old=json.loads((source/'guidance-baseline.json').read_text())
    for row in fit['decorations']:
        c=labels[row['actor']].get_component_by_class(unreal.StaticMeshComponent)
        assert c.get_world_transform().export_text()==row['transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
    hidden=0;retained=0
    for row in fit['components']:
        if row['class_name']!='TextRenderComponent' or row['actor']=='Aurelion_Art_Sign_Z04_0e7c69':continue
        c=labels[row['actor']].get_component_by_class(unreal.TextRenderComponent)
        assert str(c.text)==row['properties']['text']
        if row['actor'].startswith('Z04_'):assert c.get_editor_property('hidden_in_game');hidden+=1
        else:
            assert str(c.get_editor_property('hidden_in_game'))==row['properties']['hidden_in_game'];retained+=1
    assert hidden==17 and retained==4 and len(fit['decorations'])==6
    c=labels[old['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert c.static_mesh.get_name()=='SM_Aurelion_KIT_Z03RouteRegister' and c.get_instance_count()==39
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
    assert c.get_world_transform().export_text()==old['component_transform'] and not c.get_editor_property('override_materials')
    poses=[]
    for i in range(c.get_instance_count()):
        t=c.get_instance_transform(i,world_space=True);r=t.rotation.rotator();p=t.translation
        assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
        poses.append(([round(p.x,2),round(p.y,2),round(p.z,2)],round(r.yaw)))
    assert sorted(poses)==sorted(route_placements())
    contacts=[];floor=labels['Z04_Floor'].get_component_by_class(unreal.StaticMeshComponent)
    # Floor support and no native cover/ramp in the marker's vertical clearance column.
    obstacles=[a.get_component_by_class(unreal.StaticMeshComponent) for name,a in labels.items() if name.startswith(('Z04_LC_','Z04_HC_')) or name in ('Z04_SouthAscent_6x12_Rise3m','Z04_WestDescent_12x6_Rise3m')]
    for p,yaw in route_placements():
        x,y,_=p;start=unreal.Vector(x,y,220);end=unreal.Vector(x,y,-1)
        hit=floor.line_trace_component(start,end,False,False,False);assert hit and abs(hit[0].z)<.02
        assert not any(body.line_trace_component(start,end,False,False,False) for body in obstacles),(p,'route under obstruction')
        contacts.append([x,y,hit[0].z])
    a=labels['Aurelion_Art_Sign_Z04_0e7c69'];text=a.get_component_by_class(unreal.TextRenderComponent)
    assert (a.get_actor_location()-unreal.Vector(6400,-9131.4,242.5)).length()<.01 and abs(a.get_actor_rotation().yaw+90)<.01
    assert str(text.text)=='MEETING ATRIUM\nRELAY OVERLOOK 04' and abs(text.world_size-14)<.01 and not text.get_editor_property('hidden_in_game')
    plate=a.get_component_by_class(unreal.StaticMeshComponent);assert plate.static_mesh.get_name()=='SM_Aurelion_KIT_Z03DestinationPlaque'
    assert (plate.get_world_location()-unreal.Vector(6400,-9125.4,210)).length()<.01 and plate.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(plate.get_collision_profile_name())=='NoCollision'
    runpy.run_path(str(root/'Scripts/Editor/check_z04_cargo.py'))['check_z04_cargo'](actors)
    return dict(registers=39,hidden_layout_labels=hidden,retained_gameplay_labels=retained,retired_decorations=6,floor_contacts=contacts,qualification='Saved art and point clearance checks; live player comprehension, capsule traversal and encounter guidance remain unqualified.')
