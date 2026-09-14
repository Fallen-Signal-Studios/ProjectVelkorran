"""Refuge visual fit, retained shared art and physical surface correspondence."""
import json
from pathlib import Path
import unreal

def remaining_refuge_art(root, expected):
    fit=json.loads((root/'Art/Source/Aurelion/Z06RefugeKit/refuge-baseline.json').read_text())
    row=next(r for r in fit['nearby_art'] if r['count']==26)
    assert sorted(expected)==sorted(row['all_transforms'])
    selected={r['index'] for r in row['nearby']};assert selected==set(range(4,11))
    return [t for i,t in enumerate(row['all_transforms']) if i not in selected]

def check_z06_refuge(world,actors):
    root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z06RefugeKit/refuge-baseline.json').read_text());labels={a.get_actor_label():a for a in actors}
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for suffix,original in [('Landing','Z06_Refuge'),('Ramp','Z06_Refuge_Ramp')]:
        a=labels['KIT_Z06_Refuge_'+suffix];old=labels[original];c=a.static_mesh_component;m=c.static_mesh;s=a.get_actor_scale3d()
        assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001
        assert (a.get_actor_location()-old.get_actor_location()).length()<.01
        r=a.get_actor_rotation();rr=old.get_actor_rotation();assert max(abs(getattr(r,k)-getattr(rr,k)) for k in ('pitch','yaw','roll'))<.001
        o,e=a.get_actor_bounds(False);oo,ee=old.get_actor_bounds(False);assert (o-oo).length()<.02 and (e-ee).length()<.02,(suffix,o,e,oo,ee)
        assert m.get_name()=='SM_Aurelion_KIT_Z06Refuge'+suffix and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    for row in fit['physical']:
        if not row['actor'].startswith('Z06_Refuge'):continue
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['transform'] and c.static_mesh.get_path_name()==row['components'][0]['mesh']
        assert a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS and str(c.get_collision_profile_name())=='BlockAll'
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    floor=next(r for r in fit['nearby_art'] if r['count']==26);c=labels[floor['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(remaining_refuge_art(root,floor['all_transforms']))
    railing=next(r for r in fit['nearby_art'] if r['count']==40);c=labels[railing['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    expected_rails=railing['all_transforms'][4:] if 'KIT_Z06_RefugeFinish_Plinth' in labels else railing['all_transforms']
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(expected_rails)
    retained_rails=len(expected_rails)
    targets=[labels['Z06_Refuge'],labels['Z06_Refuge_Ramp']];ignored=[a for a in actors if a not in targets];probes=[]
    for row in fit['surface_probes']:
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(row['x'],row['y'],-350),unreal.Vector(row['x'],row['y'],-700),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        blocked=bool(h and h.to_tuple()[0]);assert blocked==row['blocked']
        if blocked:
            t=h.to_tuple();assert abs(t[5].z-row['z'])<.01 and t[9].get_actor_label()==row['actor'] and (t[6]-unreal.Vector(*row['normal'])).length()<.001
        probes.append(dict(x=row['x'],y=row['y'],blocked=blocked))
    return dict(placements=2,removed_floor_instances=7,retained_floor_instances=19,retained_railing_instances=retained_rails,surface_probes=probes,qualification='Stopped-editor fit and 50 isolated physical probes; live traversal, rescue interaction and visual/performance acceptance remain open.')
