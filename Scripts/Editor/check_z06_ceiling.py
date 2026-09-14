"""Fitted coffer dimensions, visible roof retirement and minimum relief clearance."""
import json
from pathlib import Path
import unreal

def check_z06_ceiling(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir())
    fit=json.loads((root/'Art/Source/Aurelion/Z06CeilingKit/ceiling-fit.json').read_text())
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);placed=[]
    for ci,col in enumerate(fit['columns']):
        for ri,y in enumerate(fit['rows']):
            a=labels[f'KIT_Z06_Ceiling_{ci:02}_{ri:02}'];p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False);c=a.static_mesh_component;m=c.static_mesh
            assert abs(p.x-col['x'])<.01 and abs(p.y-y)<.01
            assert abs(o.z+e.z-100)<.01 and o.z-e.z+600>=644.99
            assert abs(e.x*2-col['width'])<.01 and abs(e.y*2-400)<.01
            assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and max(abs(v) for v in (r.pitch,r.yaw,r.roll))<.001
            assert m.get_name()=='SM_Aurelion_KIT_'+col['suffix']
            assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
            assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
            assert sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
            placed.append(dict(actor=a.get_actor_label(),minimum_clearance_cm=o.z-e.z+600))
    row=fit['roof_baseline'];a=labels[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
    assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    return dict(placements=placed,retained_hidden_roof_instances=126,qualification='Stopped-editor visual assembly; all original collision retained. Live traversal, wall-running, lighting and GPU performance remain unqualified.')
