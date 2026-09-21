"""Replace the malformed 416-panel departure dressing with oriented custom bays.

Native room bodies, gameplay actors and the south doorway remain unchanged.
Preview by default; save only after a reviewed preview, then verify exact reload.
"""
from pathlib import Path
import hashlib,itertools,json,os,runpy,shutil
import unreal

root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z12DepartureWalls'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(actorsub.get_all_level_actors())
baseline=json.loads((source/'wall-baseline.json').read_text());specs=json.loads((source/'manifest.json').read_text())['modules']
owner=next(a for a in actors if a.get_actor_label()==baseline['actor'])
original=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert original.static_mesh.get_path_name()==baseline['mesh'] and original.get_instance_count()==416
assert [original.get_instance_transform(i,world_space=True).export_text() for i in range(416)]==[r['transform'] for r in baseline['instances']]
assert original.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state']([a for a in actors if a!=owner])
owner_pose=owner.get_actor_transform().export_text();owner_collision=owner.get_actor_enable_collision()
def native_state(actor):
    return sorted((c.get_path_name(),c.get_world_transform().export_text(),str(c.get_collision_enabled()))
        for c in actor.get_components_by_class(unreal.PrimitiveComponent) if not isinstance(c,unreal.InstancedStaticMeshComponent))
native_before=native_state(owner);walls={}
for row in baseline['native_walls']:
    actor=next(a for a in actors if a.get_actor_label()==row['actor'])
    assert actor.get_actor_transform().export_text()==row['transform'] and actor.get_actor_enable_collision()==row['collision']
    origin,extent=actor.get_actor_bounds(False)
    assert origin.export_text()==row['origin'] and extent.export_text()==row['extent']
    walls[row['actor']]=(origin,extent)

placements={'South':[],'North':[],'Side':[]}
for side in (-1,1):
    for index in range(4):
        for z in (150,450):
            for yaw in (0,180):placements['South'].append((('Z12_Wall_S'+str(side)),(side*(525+index*450),46300,z),yaw))
for index in range(10):
    for z in (150,450):
        for yaw in (0,180):placements['North'].append(('Z12_Wall_N',(-1890+index*420,48700,z),yaw))
for side in (-1,1):
    for index in range(6):
        for z in (150,450):
            for yaw in (-90,90):placements['Side'].append(('Z12_Wall_EW'+str(side),(side*2100,46500+index*400,z),yaw))
assert {k:len(v) for k,v in placements.items()}=={'South':32,'North':40,'Side':48}
persist=bool(globals().get('SAVE_DEPARTURE_WALLS',False));review=None
if persist:
    review=json.loads(Path(globals()['REVIEWED_DEPARTURE_FIT']).read_text())
    assert review['status']=='unsaved_preview' and review['all_module_bounds_inside_native_walls']
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
parent=next(h for h in sub.k2_gather_subobject_data_for_instance(owner) if lib.get_associated_object(lib.get_data(h))==owner)
expected={};mesh_hashes={};owner.modify()
for index,spec in enumerate(specs):
    asset=dest+'/Meshes/'+spec['asset'];file=root/('Content/'+asset.removeprefix('/Game/')+'.uasset')
    if persist:assert hashlib.sha256(file.read_bytes()).hexdigest()==review['mesh_hashes'][asset]
    mesh=unreal.load_asset(asset) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    assert mesh
    mesh_hashes[asset]=hashlib.sha256(file.read_bytes()).hexdigest()
    if index==0:c=original
    else:
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.HierarchicalInstancedStaticMeshComponent,conform_transform_to_parent=True))
        assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,'DepartureCoffer'+spec['wall_group']);c=lib.get_associated_object(lib.get_data(h))
    c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
    c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
    c.set_visibility(True);c.set_hidden_in_game(False)
    b=mesh.get_bounds();expected[asset]=[]
    for wall,position,yaw in placements[spec['wall_group']]:
        t=unreal.Transform(location=unreal.Vector(*position),rotation=unreal.Rotator(yaw=yaw),scale=unreal.Vector(1,1,1))
        origin,extent=walls[wall]
        for signs in itertools.product((-1,1),repeat=3):
            p=unreal.MathLibrary.transform_location(t,unreal.Vector(*[getattr(b.origin,k)+signs[i]*getattr(b.box_extent,k) for i,k in enumerate(('x','y','z'))]))
            assert all(abs(getattr(p,k)-getattr(origin,k))<=getattr(extent,k)+.1 for k in ('x','y','z')),(wall,p)
        c.add_instance(t,world_space=True)
    expected[asset]=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
assert helper['snapshot_actor_state']([a for a in actors if a!=owner])==before
assert owner.get_actor_transform().export_text()==owner_pose and owner.get_actor_enable_collision()==owner_collision and native_state(owner)==native_before
if persist:assert expected==review['expected']
report=dict(status='unsaved_preview',old_instances=416,new_instances=120,expected=expected,mesh_hashes=mesh_hashes,
    all_module_bounds_inside_native_walls=True,all_other_actor_transforms_and_collision_preserved=True,
    qualification='Owned visual dressing only; existing collision, doorway, props and gameplay actors retained. Live review and GPU cost remain separate.')
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(actorsub.get_all_level_actors());restored=next(a for a in current if a.get_actor_label()==baseline['actor'])
    assert helper['snapshot_actor_state']([a for a in current if a!=restored])==before
    assert restored.get_actor_transform().export_text()==owner_pose and restored.get_actor_enable_collision()==owner_collision and native_state(restored)==native_before
    actual={}
    for c in restored.get_components_by_class(unreal.InstancedStaticMeshComponent):
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        actual[c.static_mesh.get_path_name().split('.')[0]]=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
    assert actual==expected and hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',protected_m12_unchanged=True)
(out/'departure-wall-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not persist,
    'M13_ROUTE_VIEWS':[('selene-background',(1350,48200,170)),('departure-north',(0,47600,170)),('departure-side',(1000,47700,220))],
    'M13_ROUTE_YAWS':{'selene-background':-90,'departure-north':90,'departure-side':0}})
