"""Authored cache presentation with unchanged native aid and physical controls."""
from pathlib import Path
import json,unreal
def check_medical_cache_art(actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/MedicalCacheKit';old=json.loads((source/'cache-baseline.json').read_text())[0];a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.visual;body=a.body;m=c.static_mesh
    assert isinstance(a,unreal.SovAurelionMedicalCache) and a.get_path_name()==old['path'] and c.get_path_name()==old['visual']
    assert a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    assert body.get_world_transform().export_text()==old['body_transform'] and [body.get_unscaled_box_extent().x,body.get_unscaled_box_extent().y,body.get_unscaled_box_extent().z]==old['body_extent']
    assert str(body.get_collision_enabled())==old['body_collision'] and str(body.get_collision_profile_name())==old['body_profile']
    assert str(a.cache_id)==old['cache_id'] and a.support.get_path_name()==old['support'] and a.is_consumed()==old['consumed']
    assert abs(a.health_fraction-old['health_fraction'])<1e-6 and abs(a.interactable.interaction_distance-old['interaction_distance'])<1e-6 and abs(a.interactable.interaction_time-old['interaction_time'])<1e-6
    assert a.label.get_world_transform().export_text()==old['label_transform'] and str(a.label.text)=='MEDICAL AID'
    assert m.get_name()=='SM_Aurelion_KIT_MedicalAidCache' and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not a.get_editor_property('hidden') and not c.get_editor_property('override_materials')
    assert (c.get_world_location()-unreal.Vector(-3050,22600,-1200)).length()<.01 and (c.get_world_scale()-unreal.Vector(1,1,1)).length()<1e-5
    r=c.get_world_rotation();assert abs(abs(r.yaw)-180)<.001 and abs(r.pitch)+abs(r.roll)<.001
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(m)
    assert sm.get_num_uv_channels(m,0)==2 and sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();contacts=[]
    for x in (-3083,-3017):
        for y in (22558,22642):
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,-1195),unreal.Vector(x,y,-1210),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
            hits=[v for v in raw if isinstance(v,unreal.HitResult)] if isinstance(raw,tuple) else [raw];assert len(hits)==1 and isinstance(hits[0],unreal.HitResult)
            h=hits[0].to_tuple();assert h[0] and h[9].get_actor_label()=='Z08_Floor' and abs(h[5].z+1200)<.01
            contacts.append(h[5].export_text())
    hit=body.line_trace_component(unreal.Vector(-3050,22500,-1140),unreal.Vector(-3050,22600,-1140),False,False,False)
    assert hit is not None and (hit[0]-unreal.Vector(-3050,22550,-1140)).length()<.01
    assert len(actors)==3140
    return dict(actor=a.get_actor_label(),feet_floor_contacts=contacts,body_front_contact=hit[0].export_text(),native_cache_fields_preserved=True,qualification='Visual replacement and static native controls; live medical use/save-reload acceptance remains separate.')
