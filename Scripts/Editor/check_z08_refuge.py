"""Detailed refuge panel placement, exact envelopes and physical protection."""
from pathlib import Path
import json,runpy,unreal

def check_z08_refuge(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z08RefugeKit';baseline=json.loads((source/'refuge-baseline.json').read_text());by_label={a.get_actor_label():a for a in actors};helper=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);rows=[]
    for row in baseline:
        a=by_label[row['actor']];c=a.get_component_by_class(unreal.StaticMeshComponent);m=c.static_mesh;t=a.get_actor_transform();prior=helper['rail_transform'](row)
        width=1 if row['scale'][0]==.5 else 2
        assert m.get_name()=='SM_Aurelion_KIT_RefugePanel_'+str(width)+'m'
        assert c.get_path_name()==row['component'] and a.get_path_name()==row['path']
        assert (t.translation-prior.translation).length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001
        assert abs(abs(sum(getattr(t.rotation,k)*getattr(prior.rotation,k) for k in ('x','y','z','w')))-1)<1e-6
        assert a.get_actor_enable_collision()==row['actor_collision'] and str(c.get_collision_enabled())==row['collision'] and str(c.get_collision_profile_name())==('BlockAll' if row['actor_collision'] else 'NoCollision'),(row['actor'],a.get_actor_enable_collision(),str(c.get_collision_enabled()),str(c.get_collision_profile_name()),row['collision'],row['profile'])
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        b=m.get_bounds();actual=helper['bounds'](helper['corners'](c.get_world_transform(),[b.origin.x,b.origin.y,b.origin.z],[b.box_extent.x,b.box_extent.y,b.box_extent.z]));expected=helper['bounds'](helper['corners'](prior,row['origin'],row['extent']))
        error=max(abs(v-w) for av,ev in zip(actual,expected) for v,w in zip(av,ev));assert error<.02,(row['actor'],error)
        assert sm.get_convex_collision_count(m)==1 and sm.get_simple_collision_count(m)==0 and sm.get_num_uv_channels(m,0)==2
        n=sm.get_nanite_settings(m);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        rows.append(dict(actor=row['actor'],width_m=width,bounds_error_cm=error,collision=str(c.get_collision_enabled())))
    assert len(rows)==17 and len(actors)==3140
    return dict(panels=rows,qualification='Detailed 1 m and 2 m modules preserve measured bounds and collision policies. Live survivor behavior remains unqualified.')

def check_refuge_access(actors):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert not unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world)
    def hit(raw):
        if raw is None:return dict(blocked=False,actor=None)
        values=[v for v in raw if isinstance(v,unreal.HitResult)] if isinstance(raw,tuple) else [raw]
        assert len(values)==1 and isinstance(values[0],unreal.HitResult)
        p=values[0].to_tuple();return dict(blocked=bool(p[0]),actor=p[9].get_actor_label() if p[9] else None)
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08RefugeKit/refuge-baseline.json'
    baffle_labels={r['actor'] for r in json.loads(source.read_text()) if r['actor_collision']};assert len(baffle_labels)==7
    contacts=[]
    for a in actors:
        if a.get_actor_label() not in baffle_labels:continue
        pos=a.get_actor_location()
        for side in (-1,1):
            row=hit(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(pos.x,pos.y+side*80,-1050),unreal.Vector(pos.x,pos.y,-1050),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True))
            assert row['blocked'] and row['actor']==a.get_actor_label(),row
            contacts.append(row)
    assert len(contacts)==14
    marks={'Tharne':(-2750,22150,-1110),'Lyessa':(-2350,22150,-1110),'Malik':(3050,22150,-1110),'WestPatient1':(-2850,22400,-1110),'WestPatient2':(-2350,22680,-1110),'EastWalker1':(2890,22580,-1110),'EastWalker2':(3200,22680,-1110)}
    capsules=[];sightlines=[]
    for name,p in marks.items():
        row=hit(unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(*p),unreal.Vector(p[0],p[1],p[2]+.01),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True));assert not row['blocked'],(name,row);capsules.append(dict(mark=name,**row))
        for origin in ((-750,19263,-1020),(0,20800,-1020),(-1000,21600,-1020),(1200,21000,-1020)):
            row=hit(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*origin),unreal.Vector(p[0],p[1],p[2]+65),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True));assert row['blocked'],(name,origin);sightlines.append(dict(mark=name,origin=origin,**row))
    root=Path(unreal.Paths.project_dir());cabinet=runpy.run_path(str(root/'Scripts/Editor/check_z08_cabinet.py'))['check_cabinet_access'](actors)
    nav=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,unreal.Vector(3050,21750,-1110),unreal.Vector(*marks['Malik']),None,None);assert nav and nav.is_valid() and not nav.is_partial()
    return dict(baffle_contacts=contacts,cast_capsules=capsules,sightlines=sightlines,west_access=cabinet,east_refuge_path=[p.export_text() for p in nav.path_points],qualification='Static physical protection, mark clearance and navigation; live AI and survivor acceptance remain unqualified.')
