"""Fit reviewed paving to the two visual cylinders, retaining native collision."""
from pathlib import Path
import hashlib,json,os,runpy,shutil
import unreal

root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z10ChamberFloor'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(actorsub.get_all_level_actors())
baseline=json.loads((source/'floor-baseline.json').read_text(encoding='utf-8-sig'))
owner=next(a for a in actors if a.get_actor_label()==baseline['actor'])
original=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert original.static_mesh.get_path_name()==baseline['mesh']
assert original.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert [original.get_instance_transform(i,world_space=True).export_text() for i in range(original.get_instance_count())]==[r['transform'] for r in baseline['instances']]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state']([a for a in actors if a!=owner])
owner_pose=owner.get_actor_transform().export_text();owner_collision=owner.get_actor_enable_collision()
native=[c for c in owner.get_components_by_class(unreal.PrimitiveComponent) if c!=original]
def native_state():
    return [(c.get_path_name(),c.get_world_transform().export_text(),str(c.get_collision_enabled()),str(c.get_collision_profile_name())) for c in native]
native_before=native_state()
def probes():
    result=[]
    for x,y in [(0,33600),(0,34200),(250,35000),(-250,35000)]:
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,-1450),unreal.Vector(x,y,-2050),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        t=hit.to_tuple() if hit else None
        result.append(dict(x=x,y=y,hit=dict(actor=t[9].get_actor_label(),position=t[5].export_text()) if t and t[0] else None))
    return result
before_probes=probes()
persist=bool(globals().get('SAVE_CHAMBER_FLOOR',False))
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_Basalt':'PavingBasalt'}.items()}
specs=json.loads((source/'manifest.json').read_text())['modules']
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
parent=next(h for h in sub.k2_gather_subobject_data_for_instance(owner) if lib.get_associated_object(lib.get_data(h))==owner)
owner.modify();expected=[]
for index,spec in enumerate(specs):
    mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    assert mesh
    b=mesh.get_bounds();location=baseline['instances'][index]['location']
    assert abs(location[2]+b.origin.z+b.box_extent.z-(-1800 if index==0 else -1780))<.2
    if index==0:c=original
    else:
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.HierarchicalInstancedStaticMeshComponent,conform_transform_to_parent=True))
        assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,'CrownmarkDais');c=lib.get_associated_object(lib.get_data(h))
    c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
    c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
    c.add_instance(unreal.Transform(location=unreal.Vector(*location),scale=unreal.Vector(1,1,1)),world_space=True)
    expected.append((mesh.get_path_name(),c.get_instance_transform(0,world_space=True).export_text()))
assert helper['snapshot_actor_state']([a for a in actors if a!=owner])==before and owner.get_actor_transform().export_text()==owner_pose and owner.get_actor_enable_collision()==owner_collision and native_state()==native_before
assert probes()==before_probes
report=dict(status='unsaved_preview',expected=expected,native_floor_probes=before_probes,all_actor_transforms_and_collision_preserved=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(actorsub.get_all_level_actors());assert helper['snapshot_actor_state']([a for a in current if a.get_actor_label()!=baseline['actor']])==before
    restored=next(a for a in current if a.get_actor_label()==baseline['actor'])
    assert restored.get_actor_transform().export_text()==owner_pose and restored.get_actor_enable_collision()==owner_collision
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert probes()==before_probes
    actual=[(c.static_mesh.get_path_name(),c.get_instance_transform(0,world_space=True).export_text()) for c in restored.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.static_mesh and c.get_instance_count()==1]
    assert sorted(actual)==sorted(expected)
    assert all(c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation') for c in restored.get_components_by_class(unreal.InstancedStaticMeshComponent))
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'chamber-floor-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('floor-detail',(650,34000,-1100))],
    M13_ROUTE_PITCHES={'floor-detail':-30}))
