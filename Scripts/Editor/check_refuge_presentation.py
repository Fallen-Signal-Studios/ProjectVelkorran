"""Mounted refuge signage and selected west-key lighting settings."""
from pathlib import Path
import json,unreal

def check_refuge_presentation(actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/RefugeSignKit';fit=json.loads((source/'presentation-fit.json').read_text());old={r['actor']:r for r in json.loads((source/'presentation-baseline.json').read_text())};by_label={a.get_actor_label():a for a in actors};rows=[];sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for name,x in [('ART_RefugeBaffle_WestSign',-2600),('ART_RefugeBaffle_EastSign',3050)]:
        a=by_label[name];t=a.get_component_by_class(unreal.TextRenderComponent);components=a.get_components_by_class(unreal.StaticMeshComponent);assert len(components)==1;c=components[0];m=c.static_mesh
        assert a.get_class()==unreal.TextRenderActor.static_class() and not a.get_actor_enable_collision()
        assert str(t.text)==old[name]['text']['value'] and t.world_size==fit['text_size']
        assert t.vertical_alignment==unreal.VerticalTextAligment.EVRTA_TEXT_CENTER and t.horizontal_alignment==unreal.HorizTextAligment.EHTA_CENTER
        assert t.text_render_color.export_text()==old[name]['text']['color'] and t.get_material(0).get_path_name()==old[name]['text']['material']
        assert t.get_editor_property('visible') and not t.get_editor_property('hidden_in_game')
        assert c.get_owner()==a and c.get_attach_parent()==t
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        rot=c.get_world_rotation();assert abs(abs(rot.yaw)-180)<.001 and abs(rot.pitch)<.001 and abs(rot.roll)<.001
        assert m.get_name()=='SM_Aurelion_KIT_RefugeSign' and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert (c.get_world_location()-unreal.Vector(x,fit['mount_y'],fit['mount_bottom_z'])).length()<.01
        assert (c.get_world_scale()-unreal.Vector(1,1,1)).length()<.001
        n=sm.get_nanite_settings(m);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        origin,extent,radius=unreal.SystemLibrary.get_component_bounds(t)
        assert abs(origin.x-x)<.02 and abs(origin.z-fit['text_center_z'])<.02
        assert extent.x<106 and extent.z<30,(name,extent)
        assert abs(t.get_world_location().y-fit['text_y'])<.01
        contacts=[]
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        for dx in (-106,106):
            for dz in (7.5,67.5):
                rear=fit['mount_y']+4;z=fit['mount_bottom_z']+dz
                raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x+dx,rear-2,z),unreal.Vector(x+dx,rear+3,z),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True)
                values=[v for v in raw if isinstance(v,unreal.HitResult)] if isinstance(raw,tuple) else [raw]
                assert len(values)==1 and isinstance(values[0],unreal.HitResult),(name,dx,dz,raw)
                h=values[0].to_tuple();assert h[0] and h[9] and h[9].get_actor_label().startswith('ART_RefugeBaffle_')
                assert abs(h[5].y-rear)<.02,(name,dx,dz,h[5].export_text(),rear)
                contacts.append(dict(actor=h[9].get_actor_label(),impact=h[5].export_text()))
        rows.append(dict(actor=name,text_bounds=origin.export_text(),text_extent=extent.export_text(),mount=c.get_world_transform().export_text(),component=c.get_path_name(),mount_contacts=contacts))
    a=by_label['ENVL_Z08_Key_02'];c=a.get_component_by_class(unreal.RectLightComponent);baseline=old[a.get_actor_label()]
    assert a.get_actor_transform().export_text()==baseline['transform']
    assert c.intensity==fit['key_intensity'] and c.attenuation_radius==fit['key_radius']
    assert c.get_light_color().export_text()==baseline['light']['color'] and c.cast_shadows==baseline['light']['shadow'] and str(c.intensity_units)==baseline['light']['units']
    assert len(actors)==3140
    return dict(signs=rows,key_intensity=c.intensity,key_radius=c.attenuation_radius,qualification='Physical sign placement and fixed-camera lighting; full route readability and packaged performance remain unqualified.')
