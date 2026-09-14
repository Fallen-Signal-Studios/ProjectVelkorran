"""Saved wall assembly, retained native contacts and doorway collision."""
import json,runpy
from pathlib import Path
import unreal
def check_z08_walls(world,actors):
    root=Path(unreal.Paths.project_dir());labels={a.get_actor_label():a for a in actors}
    fit=json.loads((root/'Art/Source/Aurelion/Z08WallKit/wall-fit.json').read_text());row=fit['old'];a=labels[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);m=c.static_mesh
    assert a.get_actor_transform().export_text()==row['actor_transform'] and c.get_world_transform().export_text()==row['component_transform']
    assert c.get_instance_count()==1 and m.get_name()=='SM_Aurelion_KIT_Z08WallAssembly'
    t=c.get_instance_transform(0,world_space=True);assert (t.translation-unreal.Vector(0,20800,-1200)).length()<.001 and t.scale3d==unreal.Vector(1,1,1)
    r=t.rotation.rotator();assert abs(r.pitch)+abs(r.yaw)+abs(r.roll)<.001
    assert not c.get_editor_property('override_materials') and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and a.get_actor_enable_collision()==row['actor_collision']
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
    assert sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    b=m.get_bounds();assert abs(b.origin.z-b.box_extent.z)<.01 and abs(b.origin.z+b.box_extent.z-700)<.01
    manifest=json.loads((root/'Art/Source/Aurelion/Z08WallKit/manifest.json').read_text())
    assert [r['original_index'] for r in manifest['interior_coverage']]==list(range(228,266))
    contacts=[]
    assert len(fit['physical'])==8
    for row in fit['physical']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert a.get_actor_enable_collision()==row['actor_collision'] and str(c.get_collision_enabled())==row['collision']
        o,e=a.get_actor_bounds(False);axis='x' if 'EW' in row['actor'] else 'y';ignored=[other for other in actors if other!=a]
        for z in (o.z-e.z*.7,o.z,o.z+e.z*.7):
            start=unreal.Vector(o.x,o.y,z);end=unreal.Vector(o.x,o.y,z);setattr(start,axis,getattr(o,axis)-getattr(e,axis)-100);setattr(end,axis,getattr(o,axis)+getattr(e,axis)+100)
            raw=unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next((v for v in raw if isinstance(v,unreal.HitResult)),None) if isinstance(raw,tuple) else raw
            assert hit and hit.to_tuple()[0] and hit.to_tuple()[9]==a
            contacts.append(dict(actor=row['actor'],z=z))
    doors=runpy.run_path(str(root/'Scripts/Editor/check_z08_vault.py'))['check_z08_vault'](world,actors)['doorway_capsules']
    assert len(actors)==3140
    return dict(wall_bays=56,lintels=2,interior_panels=38,physical_contacts=contacts,doorway_capsules=doors,qualification='Stopped-editor asset fit and native collision; live wall-running, routes and performance remain unqualified.')
