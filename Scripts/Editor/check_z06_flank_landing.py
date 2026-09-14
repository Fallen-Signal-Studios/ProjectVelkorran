"""Landing visual, retained HISM ownership and original traversal contacts."""
from pathlib import Path
import json
import unreal

def remaining_flank_art(root,expected):
    fit=json.loads((root/'Art/Source/Aurelion/Z06FlankLandingKit/landing-baseline.json').read_text())
    assert sorted(expected)==sorted(fit['art']['all_instance_transforms'])
    index=fit['selected']['index'];assert index==3
    return [t for i,t in enumerate(fit['art']['all_instance_transforms']) if i!=index]

def check_z06_flank_landing(world,actors):
    root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z06FlankLandingKit/landing-baseline.json').read_text());labels={a.get_actor_label():a for a in actors}
    row=fit['physical'];old=labels[row['actor']];pc=old.static_mesh_component
    assert old.get_actor_transform().export_text()==row['actor_transform'] and pc.static_mesh.get_path_name()==row['mesh']
    assert old.get_actor_enable_collision() and str(pc.get_collision_enabled())==row['collision'] and str(pc.get_collision_profile_name())==row['profile']
    assert not pc.get_editor_property('visible') and pc.get_editor_property('hidden_in_game')
    a=labels['KIT_Z06_FlankLanding'];c=a.static_mesh_component;m=c.static_mesh;r=a.get_actor_rotation()
    assert (a.get_actor_location()-old.get_actor_location()-unreal.Vector(-2.5,0,0)).length()<.01 and (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001
    assert abs(r.yaw-90)+abs(r.pitch)+abs(r.roll)<.001
    o,e=a.get_actor_bounds(False);oo,ee=old.get_actor_bounds(False);assert (o-oo-unreal.Vector(-2.5,0,0)).length()<.02 and (e-ee-unreal.Vector(2.5,0,0)).length()<.02
    assert m.get_name()=='SM_Aurelion_KIT_Z06FlankLanding' and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_simple_collision_count(m)==0
    art=fit['art'];owner=labels[art['actor']];c=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert owner.get_actor_transform().export_text()==art['actor_transform'] and c.get_world_transform().export_text()==art['component_transform'] and c.static_mesh.get_path_name()==art['mesh']
    assert sorted(c.get_instance_transform(i,True).export_text() for i in range(c.get_instance_count()))==sorted(remaining_flank_art(root,art['all_instance_transforms']))
    ignored=[actor for actor in actors if actor!=old];probes=[]
    for x in (-1350,-1300,-1250):
        for y in (9500,9580,9660):
            for start,end,expected in [(-150,-450,-300),(-450,-150,-320)]:
                raw=unreal.SystemLibrary.line_trace_single_by_profile(world,unreal.Vector(x,y,start),unreal.Vector(x,y,end),'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
                h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
                assert h and h.to_tuple()[0] and h.to_tuple()[9]==old and abs(h.to_tuple()[5].z-expected)<.01
                probes.append(dict(x=x,y=y,z=expected))
    # The five-centimetre decorative seam has no new collision; sweep the existing
    # capsule envelope above both native supports to detect a new protrusion.
    seam_lanes=[]
    for y in (9530,9580,9630):
        raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(-1430,y,-205),unreal.Vector(-1280,y,-205),42,88,'Pawn',False,[actor for actor in actors if actor not in (old,labels['Aurelion_E3_ClimbWall'])],unreal.DrawDebugTrace.NONE,True)
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert not(h and h.to_tuple()[0]);seam_lanes.append(y)
    return dict(placements=1,removed_instances=1,retained_floor_instances=c.get_instance_count(),physical_contacts=probes,clear_seam_capsules=seam_lanes,visual_seam_closure_cm=5,qualification='Exact editor fit and original top/soffit contacts; live climb, route and final art remain unqualified.')
