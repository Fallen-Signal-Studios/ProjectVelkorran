"""Replace decorative departure paving while preserving native walking surfaces."""
from pathlib import Path
import hashlib,json,os,runpy,shutil
import unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z12DeparturePaving'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(actorsub.get_all_level_actors())
baseline=json.loads((source/'envelope-baseline.json').read_text(encoding='utf-8-sig'))
groups={'Lounge':('Aurelion_Art_M13_Z12_5_9dba0c','HierarchicalInstancedStaticMesh1',[(0,47500,0)]),
    'Dock':('Aurelion_Art_M13_Z12_18_14069f','HierarchicalInstancedStaticMesh',[(-3800,47500,0),(3800,47500,0)])}
owners={k:next(a for a in actors if a.get_actor_label()==v[0]) for k,v in groups.items()}
targets={k:next(c for c in owners[k].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==v[1]) for k,v in groups.items()}
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state'](actors)

def retained_components():
    rows={}
    for actor in owners.values():
        for c in actor.get_components_by_class(unreal.PrimitiveComponent):
            if c in targets.values():continue
            row=dict(transform=c.get_world_transform().export_text(),collision=str(c.get_collision_enabled()),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'))
            if isinstance(c,unreal.InstancedStaticMeshComponent):row.update(mesh=c.static_mesh.get_path_name(),instances=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())])
            rows[c.get_path_name()]=row
    return rows
retained=retained_components()
for k,(label,name,placements) in groups.items():
    row=next(a for a in baseline['actors'] if a['actor']==label)
    old=next(c for c in row['components'] if c['name']==name);c=targets[k]
    assert owners[k].get_actor_transform().export_text()==row['transform']
    assert c.static_mesh.get_path_name()==old['mesh'] and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==old['transforms']
    assert str(c.get_collision_enabled())==old['collision']=='<CollisionEnabled.NO_COLLISION: 0>'

def probes():
    rows=[]
    points=[(x,y) for x in (-1500,500,1500) for y in (46500,47500,48500)]
    points += [(x,y) for x in (-4500,-3000,3000,4500) for y in (46700,48300)]
    for x,y in points:
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,100),unreal.Vector(x,y,-100),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        t=hit.to_tuple() if hit else None
        if abs(x)>2100:
            # The saved M_GB_scenic docks explicitly have NoCollision.
            assert not t or not t[0],(x,y,'Scenic dock unexpectedly blocks')
            rows.append(dict(x=x,y=y,actor=None,position=None));continue
        assert t and t[0],(x,y,'No physical lounge floor')
        assert t[9].get_actor_label()=='Z12_Floor' and abs(t[5].z)<.1,(x,y,t[9].get_actor_label(),t[5])
        rows.append(dict(x=x,y=y,actor=t[9].get_actor_label(),position=t[5].export_text()))
    return rows
physical=probes();persist=bool(globals().get('SAVE_DEPARTURE_PAVING',False))
maps={n:hashlib.sha256((root/'Content/Aurelion/Maps'/n).read_bytes()).hexdigest() for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
review=json.loads(Path(globals()['REVIEWED_DEPARTURE_PAVING']).read_text()) if persist else None
if persist:assert review['status']=='unsaved_preview' and review['map_hashes_before']==maps
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_Basalt':'PavingBasalt'}.items()}
expected={};mesh_hashes={}
for spec in json.loads((source/'manifest.json').read_text())['modules']:
    k=spec['group'];asset=dest+'/Meshes/'+spec['asset'];file=root/('Content/'+asset.removeprefix('/Game/')+'.uasset')
    if persist:assert hashlib.sha256(file.read_bytes()).hexdigest()==review['mesh_hashes'][asset]
    mesh=unreal.load_asset(asset) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    b=mesh.get_bounds();assert abs(b.origin.z+b.box_extent.z)<.02 and abs(b.origin.x)<.02 and abs(b.origin.y)<.02
    mesh_hashes[asset]=hashlib.sha256(file.read_bytes()).hexdigest()
    c=targets[k];owners[k].modify();c.modify();c.clear_instances();c.set_static_mesh(mesh)
    c.set_editor_property('override_materials',[]);c.set_editor_property('can_ever_affect_navigation',False)
    for position in groups[k][2]:
        label='Z12_Floor' if k=='Lounge' else 'Z12_Dominion_Dock' if position[0]<0 else 'Z12_Reformation_Dock'
        native=next(a for a in actors if a.get_actor_label()==label);origin,extent=native.get_actor_bounds(False)
        assert all(abs(position[i]+getattr(b.origin,axis)-getattr(origin,axis))<.02 and abs(getattr(b.box_extent,axis)-getattr(extent,axis))<.02 for i,axis in enumerate(('x','y')))
        assert abs(position[2]+b.origin.z+b.box_extent.z-origin.z-extent.z)<.02
        assert position[2]+b.origin.z-b.box_extent.z>=origin.z-extent.z-.02
        c.add_instance(unreal.Transform(location=unreal.Vector(*position),scale=unreal.Vector(1,1,1)),world_space=True)
    expected[k]=dict(actor=groups[k][0],component=c.get_name(),mesh=mesh.get_path_name(),transforms=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())])
assert helper['snapshot_actor_state'](actors)==before and retained_components()==retained and probes()==physical
if persist:assert expected==review['expected']
report=dict(status='unsaved_preview',expected=expected,mesh_hashes=mesh_hashes,map_hashes_before=maps,native_floor_probes=physical,
    all_actor_transforms_and_collision_preserved=True,hidden_and_other_components_preserved=True,walking_surface_z=0)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();actors=list(actorsub.get_all_level_actors())
    assert helper['snapshot_actor_state'](actors)==before and probes()==physical
    owners={k:next(a for a in actors if a.get_actor_label()==v[0]) for k,v in groups.items()}
    targets={k:next(c for c in owners[k].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==v[1]) for k,v in groups.items()}
    for k,c in targets.items():
        row=expected[k];assert c.static_mesh.get_path_name()==row['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['transforms']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
    assert retained_components()==retained and hashlib.sha256((root/'Content/Aurelion/Maps/L_Aurelion_M12.umap').read_bytes()).hexdigest()==maps['L_Aurelion_M12.umap']
    report.update(status='saved_reloaded',protected_m12_unchanged=True)
(out/'departure-paving-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not persist,
    'M13_ROUTE_VIEWS':[('floor-overview',(0,47900,450)),('paving-detail',(500,48000,170)),('dock',(3800,49000,1800)),('selene-background',(1350,48200,170))],
    'M13_ROUTE_YAWS':{'floor-overview':-90,'paving-detail':-90,'dock':-90,'selene-background':-90},
    'M13_ROUTE_PITCHES':{'floor-overview':-30,'paving-detail':-35,'dock':-50}})
