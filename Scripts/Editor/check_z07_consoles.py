"""Custom request lecterns retain the authoritative request and collision owners."""
from pathlib import Path
import json,re
import unreal

def check_z07_requests(world,actors):
    root=Path(unreal.Paths.project_dir());baseline=json.loads((root/'Art/Source/Aurelion/Z07ConsoleKit/console-fit.json').read_text());labels={a.get_actor_label():a for a in actors};rows=[]
    owners=[labels[row['actor']] for row in baseline['requests']];sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for row in baseline['requests']:
        a=labels[row['actor']];handoff='HandoffToSeleneCage' in row['actor'];v=a.visual;b=a.body
        assert a.get_actor_transform().export_text()==row['transform']
        for key,expected in row['properties'].items():
            value=a.get_editor_property(key);value=value.get_path_name() if isinstance(value,unreal.Object) else str(value)
            assert value==expected,(row['actor'],key)
        components={c.get_name():c for c in a.get_components_by_class(unreal.SceneComponent)}
        for old in row['components']:
            if old['name']=='Visual':continue
            c=components[old['name']]
            actual=c.get_world_transform().export_text()
            if old['name']=='StaticMesh':
                # Reparent-relative conversion can change quaternion sign or signed zero.
                av=[float(v) for v in re.findall(r'[-+]?\d+\.\d+',actual)];bv=[float(v) for v in re.findall(r'[-+]?\d+\.\d+',old['world'])]
                assert len(av)==len(bv)==10
                assert max(abs(a-b) for a,b in zip(av[4:],bv[4:]))<.001 and abs(abs(sum(a*b for a,b in zip(av[:4],bv[:4])))-1)<.00001,(row['actor'],old['name'],actual,old['world'])
            else:assert actual==old['world'],(row['actor'],old['name'],actual,old['world'])
            if old['name']!='StaticMesh':assert c.get_relative_transform().export_text()==old['relative']
            if 'collision' in old:assert str(c.get_collision_enabled())==old['collision'] and str(c.get_collision_profile_name())==old['profile']
            if old['name']=='StaticMesh':assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        assert (b.get_scaled_box_extent()-unreal.Vector(25,35,45)).length()<.001 and v.get_attach_parent()==b
        t=v.get_relative_transform();assert (t.translation-unreal.Vector(0,12.068254,-5 if handoff else 0)).length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001
        rotation=v.get_editor_property('relative_rotation');assert abs(abs(rotation.yaw)-0)<.001 and abs(rotation.pitch)+abs(rotation.roll)<.001
        m=v.static_mesh;assert m.get_name()=='SM_Aurelion_KIT_RequestConsoleRaisedBase'
        assert v.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and v.get_editor_property('visible') and not v.get_editor_property('hidden_in_game')
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_simple_collision_count(m)==0
        origin,extent=v.get_local_bounds();bottom=unreal.MathLibrary.transform_location(v.get_world_transform(),unreal.Vector(0,0,origin.z))
        raw=unreal.SystemLibrary.line_trace_single_by_profile(world,bottom+unreal.Vector(0,0,5),bottom-unreal.Vector(0,0,10),'Pawn',False,owners,unreal.DrawDebugTrace.NONE,True)
        h=next(x for x in raw if isinstance(x,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert h and h.to_tuple()[0] and abs(h.to_tuple()[5].z-bottom.z)<.1
        rows.append(dict(actor=row['actor'],mesh=m.get_name(),bottom_cm=bottom.z,floor_cm=h.to_tuple()[5].z))
    return dict(actors=rows,qualification='Saved visual, grounding and unchanged request bindings/body; native interaction and final art require live acceptance.')

def check_z07_consoles(world,actors):
    for name in ('UplightLens','StoneGrout'):
        assert unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+name).get_editor_property('used_with_nanite')
    requests=check_z07_requests(world,actors);root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z07ConsoleKit/console-fit.json').read_text());labels={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for row in fit['priorities']:
        a=labels[row['actor']];assert a.get_actor_transform().export_text()==row['transform']
        expected=unreal.SovAurelionRescuePriority.WEST_STRETCHERS if 'West' in row['actor'] else unreal.SovAurelionRescuePriority.EAST_WALKERS
        assert a.get_editor_property('priority')==expected
        components={c.get_name():c for c in a.get_components_by_class(unreal.SceneComponent)}
        for old in row['components']:
            c=components[old['name']];assert c.get_world_transform().export_text()==old['world'] and c.get_relative_transform().export_text()==old['relative']
            if 'collision' in old:assert str(c.get_collision_enabled())==old['collision'] and str(c.get_collision_profile_name())==old['profile']
        assert (a.body.get_scaled_box_extent()-unreal.Vector(25,35,60)).length()<.001
        assert a.get_editor_property('interactable').get_owner()==a
    for row in fit['art']:
        a=labels[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);desk='_67_' in row['actor'];m=c.static_mesh
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.get_world_transform().export_text()==row['component_transform']
        assert c.get_instance_count()==2 and m.get_name()=='SM_Aurelion_KIT_Z07'+('ControlDesk' if desk else 'PriorityConsole')
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('override_materials')
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_convex_collision_count(m)==0
        for i,x in enumerate((-100,100) if desk else (-120,120)):
            t=c.get_instance_transform(i,world_space=True);p=t.translation;r=t.rotation.rotator();s=t.scale3d
            assert (p-unreal.Vector(x,15200 if desk else 15700,-900 if desk else -839)).length()<.001
            assert abs(r.yaw-(180 if desk else 90))<.001 and abs(r.pitch)+abs(r.roll)<.001 and (s-unreal.Vector(1,1,1)).length()<.001
    row=fit['desk'];a=labels[row['actor']];assert a.get_actor_transform().export_text()==row['actor_transform'] and a.static_mesh_component.static_mesh.get_path_name()==row['mesh']
    assert str(a.static_mesh_component.get_collision_enabled())==row['collision'] and a.get_actor_enable_collision()==row['actor_collision']
    return dict(requests=requests,priority_terminals=2,desk_modules=2,qualification='Saved visual and native request/choice owner checks; actual focus, hold input, scene playback and final visual acceptance remain unqualified.')
