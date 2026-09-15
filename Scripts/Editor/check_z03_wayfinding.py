"""Saved wayfinding, retired decorative collision and preserved scanner cues."""
from pathlib import Path
import json,runpy,unreal

def check_z03_wayfinding(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z03WayfindingKit';fit=json.loads((source/'presentation-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text());labels={a.get_actor_label():a for a in actors}
    for row in fit['decorations']:
        c=labels[row['actor']].get_component_by_class(unreal.StaticMeshComponent)
        assert c.get_path_name()==row['component'] and c.get_world_transform().export_text()==row['transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert c.get_editor_property('visible') and c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision',row['actor']
    hidden=[];retained=[]
    for row in fit['components']:
        if row['class_name']!='TextRenderComponent':continue
        a=labels[row['actor']];c=a.get_component_by_class(unreal.TextRenderComponent)
        assert c.get_path_name()==row['component'] and c.get_editor_property('visible')
        if row['actor'].startswith('Z03_'):
            assert c.get_editor_property('hidden_in_game') and str(c.text)==row['properties']['text'];hidden.append(row['actor'])
        elif row['actor'].startswith('Aurelion_Z03_SweepScanner'):
            assert not c.get_editor_property('hidden_in_game') and str(c.text)==row['properties']['text']
            assert (c.get_world_location()-unreal.Vector(*row['location'])).length()<.001;retained.append(row['actor'])
    old=json.loads((source/'guidance-baseline.json').read_text());a=labels[old['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert a.get_actor_transform().export_text()==old['actor_transform'] and c.get_world_transform().export_text()==old['component_transform'] and c.get_instance_count()==44 and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert c.static_mesh.get_name()=='SM_Aurelion_KIT_Z03RouteRegister' and not c.get_editor_property('override_materials')
    for i,row in enumerate(manifest['placements']):
        assert row['location_cm']==[7200,-19550+100*i,-.10]
        t=c.get_instance_transform(i,world_space=True)
        assert (t.translation-unreal.Vector(*row['location_cm'])).length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001
        r=t.rotation.rotator();assert abs(r.yaw)+abs(r.pitch)+abs(r.roll)<.001
        hit=labels['Z03_Floor'].get_component_by_class(unreal.StaticMeshComponent).line_trace_component(unreal.Vector(7200,-19550+100*i,20),unreal.Vector(7200,-19550+100*i,-40),False,False,False)
        assert hit is not None and abs(hit[0].z)<.02
    for name in ('Z03_Frozen_Rib_12','Z03_Frozen_Rib_24','Z03_Frozen_Rib_35'):
        origin,extent=labels[name].get_actor_bounds(False)
        assert 7200-6-(origin.x+extent.x)>=194-.01
    sign=labels['Aurelion_Art_Sign_Z03_5250c2'];text=sign.get_component_by_class(unreal.TextRenderComponent);plate=sign.get_component_by_class(unreal.StaticMeshComponent)
    assert str(text.text)=='RELAY OVERLOOK\nSENSOR GALLERY 03' and abs(text.world_size-14)<.001 and not text.get_editor_property('hidden_in_game')
    assert (text.get_world_location()-unreal.Vector(7600,-15051,242.5)).length()<.001
    assert text.get_editor_property('vertical_alignment')==unreal.VerticalTextAligment.EVRTA_TEXT_CENTER
    assert plate.static_mesh.get_name()=='SM_Aurelion_KIT_Z03DestinationPlaque' and plate.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert (plate.get_world_location()-unreal.Vector(7600,-15045,210)).length()<.001
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for mesh in (c.static_mesh,plate.static_mesh):
        n=sm.get_nanite_settings(mesh);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
    runpy.run_path(str(root/'Scripts/Editor/check_z03_floors.py'))['check_z03_floors'](actors)
    return dict(route_registers=44,route_floor_contacts=44,minimum_cover_clearance_cm=194,retired_decorative_colliders=len(fit['decorations']),hidden_setup_labels=hidden,retained_scanner_cues=retained,wall_destination_signs=1,qualification='Saved art and collision cleanup; live navigation, scanner legibility and packaged performance remain unqualified.')
