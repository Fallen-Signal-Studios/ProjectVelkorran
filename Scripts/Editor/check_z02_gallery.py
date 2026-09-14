"""Custom gallery placement and retained mesh collision inspection."""
import json
from pathlib import Path
import unreal

def placements(fit):
    rows=[]
    for i,b in enumerate(fit['baseline']):
        if 'Trim' in b['actor']:continue
        x,y,z=b['origin'];bottom=z-b['extent'][2]
        suffix='Z02ReliefPanel' if 'Window' in b['actor'] else 'Z02BranchPier'
        rows.append((str(i),suffix,(x,y,bottom),-90 if x<-7000 else 90))
    return rows

def check_gallery(world,actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z02GalleryKit';fit=json.loads((source/'gallery-fit.json').read_text(encoding='utf-8-sig'))
    labels={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);checked=[];queries=[];pier_queries=[]
    for name,suffix,pos,yaw in placements(fit):
        a=labels['KIT_Z02_Gallery_'+name];c=a.static_mesh_component;p=a.get_actor_location();scale=a.get_actor_scale3d()
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),pos))<.01
        assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z)) and abs(a.get_actor_rotation().yaw-yaw)<.01
        assert c.static_mesh.get_name()=='SM_Aurelion_KIT_'+suffix and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        is_pier=suffix=='Z02BranchPier'
        assert a.get_actor_enable_collision()==is_pier
        assert c.get_collision_enabled()==(unreal.CollisionEnabled.QUERY_AND_PHYSICS if is_pier else unreal.CollisionEnabled.NO_COLLISION)
        assert sm.get_simple_collision_count(c.static_mesh)==0 and sm.get_convex_collision_count(c.static_mesh)==(14 if is_pier else 0)
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
        if is_pier:
            spec=next(s for s in json.loads((source/'manifest.json').read_text())['modules'] if s['asset']==c.static_mesh.get_name())
            ignored=[other for other in actors if other!=a];t=a.get_actor_transform()
            for x,z,expected in spec['pier_collision_samples']:
                start=unreal.MathLibrary.transform_location(t,unreal.Vector(x*100,-500,z*100));end=unreal.MathLibrary.transform_location(t,unreal.Vector(x*100,500,z*100))
                raw=unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
                hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
                assert bool(hit and hit.to_tuple()[0])==expected,(name,x,z,expected)
                pier_queries.append(dict(actor=a.get_actor_label(),x=x,z=z,blocked=expected))
            for x,expected in ((0,True),(125,False)):
                start=unreal.MathLibrary.transform_location(t,unreal.Vector(x,-150,150));end=unreal.MathLibrary.transform_location(t,unreal.Vector(x,150,150))
                raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
                hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
                assert bool(hit and hit.to_tuple()[0])==expected,(name,x,expected)
                pier_queries.append(dict(actor=a.get_actor_label(),capsule_x=x,blocked=expected))
        if suffix=='Z02ReliefPanel':
            o,e=a.get_actor_bounds(False);assert abs(o.z-e.z-7.091656)<.05,'Relief footing does not meet paving'
        checked.append(a.get_actor_label())
    for b in fit['baseline']:
        a=labels[b['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==b['transform'] and c.static_mesh.get_path_name()==b['mesh']
        assert str(c.get_collision_enabled())==b['collision'] and str(c.get_collision_profile_name())==b['profile']
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        if 'Trim' in b['actor']:continue
        x,y,z=b['origin'];ignored=[other for other in actors if other!=a]
        for height in (z-b['extent'][2]+50,z,z+b['extent'][2]-50):
            for complex_trace in (False,True):
                raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x-200,y,height),unreal.Vector(x+200,y,height),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex_trace,ignored,unreal.DrawDebugTrace.NONE,True)
                hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
                blocking=bool(hit and hit.to_tuple()[0]);queries.append(dict(actor=b['actor'],z=height,complex=complex_trace,blocking=blocking,impact=hit.to_tuple()[5].export_text() if blocking else None))
    return dict(placements=checked,authored_pier_queries=pier_queries,retained_mesh_queries=queries,qualification='Original contracts preserved and new pier simple collision validated by isolated rays/capsules; live movement, navigation and final surface correspondence remain unqualified.')
