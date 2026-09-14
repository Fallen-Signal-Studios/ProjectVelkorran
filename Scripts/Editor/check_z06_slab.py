"""Fallen masonry fit, retained art ownership and surrounding passage baseline."""
import json,math,runpy
from pathlib import Path
import unreal

def check_z06_slab(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z06FallenSlabKit/slab-baseline.json').read_text())
    old=labels[fit['physical']['actor']];a=labels['KIT_Z06_FallenMasonry'];c=a.static_mesh_component;m=c.static_mesh;s=a.get_actor_scale3d()
    assert old.get_actor_transform().export_text()==fit['physical']['transform'] and old.static_mesh_component.static_mesh.get_path_name()==fit['physical']['mesh']
    assert old.get_actor_enable_collision() and old.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and abs(a.get_actor_rotation().yaw-old.get_actor_rotation().yaw)<.001
    assert (a.get_actor_location()-unreal.Vector(0,8600,-600)).length()<.01
    o,e=a.get_actor_bounds(False);oo,ee=old.get_actor_bounds(False);assert (o-oo).length()<.02 and (e-ee).length()<.02
    assert m.get_name()=='SM_Aurelion_KIT_Z06FallenMasonry' and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    row=next(r for r in fit['art'] if r['count']==48);selected={r['index'] for r in row['selected']};expected=[t for i,t in enumerate(row['all_transforms']) if i not in selected]
    c=labels[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    if 'KIT_Z06_Refuge_Landing' in labels:
        expected=runpy.run_path(str(root/'Scripts/Editor/check_z06_refuge.py'))['remaining_refuge_art'](root,expected)
    if 'KIT_Z06_FlankLanding' in labels:
        expected=runpy.run_path(str(root/'Scripts/Editor/check_z06_flank_landing.py'))['remaining_flank_art'](root,expected)
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(expected)
    retained_count=len(expected)
    probes=[]
    for row in fit['passage_probes']:
        raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(row['x'],row['start_y'],row['z']),unreal.Vector(row['x'],row['end_y'],row['z']),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        blocked=bool(h and h.to_tuple()[0]);actor=h.to_tuple()[9].get_actor_label() if blocked and h.to_tuple()[9] else None
        expected_actor=row['actor']
        if 'KIT_Z06_GateHousing' in labels and row['x']==900:
            # This already-blocked lane meets the new west jamb before the closed leaf.
            assert row['blocked'] and row['actor']=='Aurelion_E3_RescueAccess'
            expected_actor='KIT_Z06_GateHousing'
            control=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(row['x'],row['start_y'],row['z']),unreal.Vector(row['x'],row['end_y'],row['z']),42,88,'Pawn',False,[labels['KIT_Z06_GateHousing']],unreal.DrawDebugTrace.NONE,True)
            control=next(v for v in control if isinstance(v,unreal.HitResult)) if isinstance(control,tuple) else control
            assert control and control.to_tuple()[0] and control.to_tuple()[9].get_actor_label()==row['actor']
        assert blocked==row['blocked'] and actor==expected_actor,(row,blocked,actor)
        probes.append(dict(x=row['x'],blocked=blocked,actor=actor))
    ignored=[a for a in actors if a!=old];tops=[];yaw=math.radians(old.get_actor_rotation().yaw)
    for x in (-450,0,450):
        for y in (-300,0,300):
            wx=x*math.cos(yaw)-y*math.sin(yaw);wy=8600+x*math.sin(yaw)+y*math.cos(yaw)
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(wx,wy,-200),unreal.Vector(wx,wy,-700),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert h and h.to_tuple()[0] and abs(h.to_tuple()[5].z+360)<.01;tops.append(h.to_tuple()[5].z)
    return dict(placements=1,removed_slab_instances=22,retained_floor_instances=retained_count,passage_probes=probes,physical_top_probes=tops,qualification='Editor bounds and physical controls; local visual spalling is cosmetic. Live traversal, combat and performance remain unqualified.')
