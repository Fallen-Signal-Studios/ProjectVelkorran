"""Replace only the 96 noncolliding visual ring blocks; scoped preview/save/readback."""
from pathlib import Path
import hashlib,json,os,runpy,shutil
import unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z10CeilingRings'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(actorsub.get_all_level_actors())
baseline=json.loads((source/'ring-baseline.json').read_text(encoding='utf-8-sig'))
owner=next(a for a in actors if a.get_actor_label()==baseline['actor'])
original=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_path_name()==baseline['component'])
assert original.static_mesh.get_path_name()==baseline['mesh']
assert original.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
transforms=[original.get_instance_transform(i,world_space=True) for i in range(original.get_instance_count())]
assert [t.export_text() for t in transforms]==baseline['instances'] and len(transforms)==96
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state']([a for a in actors if a!=owner])
pose=owner.get_actor_transform().export_text();collision=owner.get_actor_enable_collision()
def native_state(actor):
    return sorted((c.get_path_name(),c.get_world_transform().export_text(),str(c.get_collision_enabled()))
                  for c in actor.get_components_by_class(unreal.PrimitiveComponent)
                  if not isinstance(c,unreal.InstancedStaticMeshComponent))
native_before=native_state(owner)
persist=bool(globals().get('SAVE_CEILING_RINGS',False))
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
specs=json.loads((source/'manifest.json').read_text())['modules']
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
parent=next(h for h in sub.k2_gather_subobject_data_for_instance(owner) if lib.get_associated_object(lib.get_data(h))==owner)
owner.modify();expected={}
for index,spec in enumerate(specs):
    mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    assert mesh
    if index==0:c=original
    else:
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.HierarchicalInstancedStaticMeshComponent,conform_transform_to_parent=True))
        assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,'CeilingRing'+str(spec['radius_m']));c=lib.get_associated_object(lib.get_data(h))
    c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
    c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
    for t in transforms[index*32:(index+1)*32]:
        t.set_editor_property('scale3d',unreal.Vector(1,1,1));c.add_instance(t,world_space=True)
    expected[mesh.get_path_name()]=[c.get_instance_transform(i,world_space=True).export_text() for i in range(32)]
assert helper['snapshot_actor_state']([a for a in actors if a!=owner])==before
assert owner.get_actor_transform().export_text()==pose and owner.get_actor_enable_collision()==collision and native_state(owner)==native_before
report=dict(status='unsaved_preview',instances=96,rings=3,expected=expected,other_actor_transforms_and_collision_preserved=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(actorsub.get_all_level_actors());restored=next(a for a in current if a.get_actor_label()==baseline['actor'])
    assert helper['snapshot_actor_state']([a for a in current if a!=restored])==before
    assert restored.get_actor_transform().export_text()==pose and restored.get_actor_enable_collision()==collision and native_state(restored)==native_before
    actual={}
    for c in restored.get_components_by_class(unreal.InstancedStaticMeshComponent):
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
        actual[c.static_mesh.get_path_name()]=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
    assert actual==expected
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'ceiling-ring-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('ceiling',(0,35000,-1570)),('ring-detail',(0,37600,-800))],
    M13_ROUTE_PITCHES={'ceiling':32,'ring-detail':22}))
