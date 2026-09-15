"""Unit-scale shell placement and isolated original-envelope collision probes."""
from pathlib import Path
import json,unreal

def shell_spec(row):
    threshold=row['actor'].endswith('ThresholdMark');length=int(max(row['scale'][:2]));location=list(row['location'])
    if threshold:location[2]=-1199.9
    return 'SM_Aurelion_KIT_Refuge'+('Threshold_' if threshold else 'Shell_')+str(length)+'m',length,location,90 if row['scale'][1]==10 else 0,threshold

def check_refuge_shell(actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/RefugeShellKit';baseline=json.loads((source/'shell-baseline.json').read_text());by_label={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);rows=[]
    for row in baseline:
        name,length,location,yaw,threshold=shell_spec(row);a=by_label[row['actor']];c=a.get_component_by_class(unreal.StaticMeshComponent);m=c.static_mesh
        assert a.get_path_name()==row['path'] and c.get_path_name()==row['meshes'][0]['path']
        assert m.get_name()==name and a.get_actor_enable_collision()==row['collision']
        assert not a.get_editor_property('hidden') and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not c.get_editor_property('override_materials')
        expected_materials={'M_AurelionKit_PavingIvory','M_AurelionKit_Gold','M_AurelionKit_Reveal','M_AurelionKit_UplightLens'}
        assert {c.get_material(i).get_name() for i in range(c.get_num_materials())}==expected_materials
        assert (a.get_actor_location()-unreal.Vector(*location)).length()<.01
        r=a.get_actor_rotation();assert abs(r.yaw-yaw)<.001 and abs(r.pitch)<.001 and abs(r.roll)<.001
        assert (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001
        assert c.get_world_transform().export_text()==a.get_actor_transform().export_text()
        assert str(c.get_collision_profile_name())==('NoCollision' if threshold else 'BlockAll')
        assert c.get_collision_enabled()==(unreal.CollisionEnabled.NO_COLLISION if threshold else unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        n=sm.get_nanite_settings(m);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==(0 if threshold else 1)
        probes=[]
        if not threshold:
            t=c.get_world_transform()
            def probe(start,end,expected):
                raw=c.line_trace_component(unreal.MathLibrary.transform_location(t,unreal.Vector(*start)),unreal.MathLibrary.transform_location(t,unreal.Vector(*end)),False,False,False)
                # K2_LineTraceComponent uses a bool return (Python tuple/None); the
                # component-only query does not set a world blocking-hit flag.
                assert isinstance(raw,tuple) and len(raw)==4,(row['actor'],start,raw)
                contact,normal=raw[:2];target=unreal.MathLibrary.transform_location(t,unreal.Vector(*expected))
                assert (contact-target).length()<.05 and abs(normal.length()-1)<.001,(row['actor'],start,raw,target.export_text())
                probes.append(dict(start=start,expected=expected,hit=contact.export_text()))
            for x in (-length*50+1,0,length*50-1):
                for z in (-149,0,149):
                    for side in (-1,1):probe((x,side*100,z),(x,0,z),(x,side*20,z))
            for side in (-1,1):
                probe((side*(length*50+100),0,0),(0,0,0),(side*length*50,0,0))
                probe((0,0,side*250),(0,0,0),(0,0,side*150))
            for x,z in ((length*50+1,0),(-length*50-1,0),(0,151),(0,-151)):
                raw=c.line_trace_component(unreal.MathLibrary.transform_location(t,unreal.Vector(x,-100,z)),unreal.MathLibrary.transform_location(t,unreal.Vector(x,100,z)),False,False,False)
                assert raw is None,(row['actor'],'Outside hull must miss',raw)
        else:
            origin,extent,radius=unreal.SystemLibrary.get_component_bounds(c)
            assert abs(origin.z+extent.z+1199.9)<.02 and abs(extent.x-length*50)<.02 and abs(extent.y-10)<.02
        rows.append(dict(actor=row['actor'],mesh=name,collision_probes=probes))
    overlay=json.loads((source/'threshold-overlay-baseline.json').read_text());a=by_label[overlay['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert a.get_path_name()==overlay['path'] and c.get_path_name()==overlay['component'] and c.static_mesh.get_path_name()==overlay['mesh']
    assert a.get_actor_transform().export_text()==overlay['actor_transform'] and c.get_world_transform().export_text()==overlay['component_transform']
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(overlay['all_transforms'][2:4])
    assert a.get_actor_enable_collision()==overlay['actor_collision'] and str(c.get_collision_enabled())==overlay['collision'] and str(c.get_collision_profile_name())==overlay['profile']
    assert [c.get_material(i).get_path_name() for i in range(c.get_num_materials())]==overlay['materials']
    assert c.get_editor_property('visible')==overlay['visible'] and c.get_editor_property('hidden_in_game')==overlay['hidden']
    return dict(placements=rows,removed_duplicate_threshold_instances=[0,1,4],preserved_guidance_instances=[2,3],scope='Ten exact original solid collision envelopes, two nonblocking embedded inlays; live rescue acceptance remains separate')
