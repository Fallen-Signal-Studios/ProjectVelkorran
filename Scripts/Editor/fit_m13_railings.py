"""Replace chamber rail artwork inside each measured envelope; leave native barriers intact."""
from pathlib import Path
import hashlib,itertools,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z10RailingKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actor_sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
old=json.loads((source/'rail-baseline.json').read_text(encoding='utf-8-sig'));fit=json.loads((source/'rail-fit.json').read_text());specs=json.loads((source/'manifest.json').read_text())['modules']
actors=list(actor_sub.get_all_level_actors());owner=next(a for a in actors if a.get_actor_label()==old['actor']);original=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert original.static_mesh.get_path_name()==old['mesh'] and original.get_instance_count()==192
assert [original.get_instance_transform(i,world_space=True).export_text() for i in range(192)]==[r['transform'] for r in old['instances']]
assert str(original.get_collision_enabled())==old['collision']
def native_state(actor):
    return sorted((c.get_class().get_path_name(),c.get_world_transform().export_text(),str(c.get_collision_enabled())) for c in actor.get_components_by_class(unreal.PrimitiveComponent) if not isinstance(c,unreal.InstancedStaticMeshComponent))
native_before=native_state(owner)
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state']([a for a in actors if a!=owner])
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
parent=next(h for h in sub.k2_gather_subobject_data_for_instance(owner) if lib.get_associated_object(lib.get_data(h))==owner)
def transform(row):
    t=unreal.Transform();t.translation=unreal.Vector(*row['location']);t.rotation=unreal.Quat(*row['quaternion']);t.scale3d=unreal.Vector(*row.get('scale',[1,1,1]));return t
def corners(t,origin,extent):
    return [unreal.MathLibrary.transform_location(t,unreal.Vector(*[origin[i]+s[i]*extent[i] for i in range(3)])) for s in itertools.product((-1,1),repeat=3)]
def bounds(points):return [[fn(getattr(p,k) for p in points) for k in ('x','y','z')] for fn in (min,max)]
persist=bool(globals().get('SAVE_M13_RAILS',False));dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
expected={};envelope_errors=[];owner.modify()
for index,spec in enumerate(specs):
    mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    if index==0:c=original
    else:
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.HierarchicalInstancedStaticMeshComponent,conform_transform_to_parent=True))
        assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,'ChamberRail'+str(index+1));c=lib.get_associated_object(lib.get_data(h))
    c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
    c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
    b=mesh.get_bounds();o=[b.origin.x,b.origin.y,b.origin.z];e=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
    expected[mesh.get_path_name()]=[]
    for row in fit['placements']:
        if row['asset']!=spec['asset']:continue
        t=transform(row)
        old_points=[p for i in row['original_indices'] for p in corners(transform(old['instances'][i]),old['mesh_origin'],old['mesh_extent'])]
        previous=bounds(old_points);current=bounds(corners(t,o,e))
        error=max(abs(a-b) for aa,bb in zip(previous,current) for a,b in zip(aa,bb));assert error<.2,(row,error)
        envelope_errors.append(error);instance=c.add_instance(t,world_space=True)
        expected[mesh.get_path_name()].append(c.get_instance_transform(instance,world_space=True).export_text())
assert helpers['snapshot_actor_state']([a for a in actors if a!=owner])==before
assert owner.get_actor_transform().export_text()==old['actor_transform'] and owner.get_actor_enable_collision()==old['actor_collision']
assert native_state(owner)==native_before
report=dict(status='unsaved_preview',original_instances=192,new_instances=len(fit['placements']),max_envelope_error_cm=max(envelope_errors),other_actors_and_native_collision_preserved=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(actor_sub.get_all_level_actors());restored=next(a for a in current if a.get_actor_label()==old['actor'])
    assert restored.get_actor_transform().export_text()==old['actor_transform'] and restored.get_actor_enable_collision()==old['actor_collision']
    assert native_state(restored)==native_before
    assert helpers['snapshot_actor_state']([a for a in current if a!=restored])==before
    actual={}
    for c in restored.get_components_by_class(unreal.InstancedStaticMeshComponent):
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
        actual[c.static_mesh.get_path_name()]=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
    assert actual==expected
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'chamber-rail-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('railing',(1700,33200,-1600)),('approach',(0,32200,-1510)),('bridge-rail',(0,38200,-1600))],
    M13_ROUTE_YAWS={'railing':-50,'bridge-rail':0},M13_ROUTE_PITCHES={'railing':-8,'approach':-8,'bridge-rail':-8}))
