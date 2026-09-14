"""Owned gate visual fit and unchanged authoritative motion components."""
import json,runpy
from pathlib import Path
import unreal
def check_z06_rescue_gate(world,actors):
    root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z06RescueGateKit/gate-baseline.json').read_text());labels={a.get_actor_label():a for a in actors};door=labels[fit['actor']];body=door.moving_body;visual=door.visual
    assert door.get_actor_transform().export_text()==fit['actor_transform'] and str(door.transit_id)==fit['transit_id'] and abs(door.travel_seconds-fit['travel_seconds'])<.001
    assert (door.destination_offset-unreal.Vector(*fit['destination_offset'])).length()<.001 and str(door.required_mission)==fit['required_mission'] and str(door.required_beat)==fit['required_beat']
    for row in fit['components']:
        if row['name']=='Visual':continue
        c=next(c for c in door.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()==row['name'])
        assert c.get_world_transform().export_text()==row['world_transform'] and c.get_relative_transform().export_text()==row['relative_transform']
        assert str(c.get_collision_enabled())==row['collision'] and str(c.get_collision_profile_name())==row['profile']
    assert (body.get_scaled_box_extent()-unreal.Vector(*fit['body_extent'])).length()<.001
    assert visual.get_attach_parent()==body and visual.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert visual.get_editor_property('visible') and not visual.get_editor_property('hidden_in_game')
    t=visual.get_relative_transform();assert t.translation.length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001
    r=visual.get_editor_property('relative_rotation');assert max(abs(r.pitch),abs(r.yaw),abs(r.roll))<.001
    mesh=visual.static_mesh;assert mesh.get_name()=='SM_Aurelion_KIT_Z06RescueGate'
    bounds=mesh.get_bounds();assert bounds.origin.length()<.001 and (bounds.box_extent-body.get_scaled_box_extent()).length()<.01
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled') and sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==0
    hit=runpy.run_path(str(root/'Scripts/Editor/validate_aurelion_transit_clearance.py'))['_hit'];start=body.get_world_location();end=unreal.MathLibrary.transform_location(door.scene_root.get_world_transform(),door.destination_offset);sweeps=[]
    for name,a,b in [('initial',start,start+unreal.Vector(0,0,.1)),('open',start,end),('close',end,start)]:
        result=hit(unreal.SystemLibrary.box_trace_single_by_profile(world,a,b,body.get_scaled_box_extent(),body.get_world_rotation(),body.get_collision_profile_name(),False,[door],unreal.DrawDebugTrace.NONE,True));assert not result['blocking'],(name,result);sweeps.append(dict(name=name,result=result))
    return dict(visual=mesh.get_path_name(),attachment='MovingBody',unit_scale=True,sweeps=sweeps,qualification='Full moving-box sweeps and exact visual envelope. Native RequestUse, wave release, live interaction and final art remain unqualified.')
