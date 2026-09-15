"""Fit four custom modules on the original wound-gallery art actor."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09WallKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
old=json.loads((source/'wall-baseline.json').read_text(encoding='utf-8-sig'));manifest=json.loads((source/'manifest.json').read_text())
a=next(a for a in actors if a.get_actor_label()==old['actor'])
original=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert original.static_mesh.get_path_name()==old['mesh']
assert [original.get_instance_transform(i,world_space=True).export_text() for i in range(original.get_instance_count())]==[r['transform'] for r in old['instances']]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
untouched=[actor for actor in actors if actor!=a];before=helper['snapshot_actor_state'](untouched)
def native_state(c):
    return (c.get_path_name(),c.get_world_transform().export_text(),c.static_mesh.get_path_name() if c.static_mesh else None,str(c.get_collision_enabled()),str(c.get_collision_profile_name()),c.get_editor_property('visible'),c.get_editor_property('hidden_in_game'))
native=[c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c!=original]
native_before=[native_state(c) for c in native]
native_file=source/'native-components.json'
if native_file.exists():assert json.loads(native_file.read_text())==[list(r) for r in native_before]
else:native_file.write_text(json.dumps(native_before,indent=2))
dest='/Game/Aurelion/Environment/ArchitectureKit';persist=bool(globals().get('PERSIST_Z09_WALLS',False))
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
parent=next(h for h in sub.k2_gather_subobject_data_for_instance(a) if lib.get_associated_object(lib.get_data(h))==a)
a.modify()
for index,spec in enumerate(manifest['modules']):
    suffix=spec['asset'].replace('SM_Aurelion_KIT_','')
    assert not json.loads((source/(suffix+'-coplanar.json')).read_text())['overlaps']
    mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    assert mesh
    if index==0:c=original
    else:
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.HierarchicalInstancedStaticMeshComponent,conform_transform_to_parent=True))
        assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,suffix);c=lib.get_associated_object(lib.get_data(h))
    c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
    c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
    for row in manifest['placements']:
        if row['asset']==spec['asset']:c.add_instance(unreal.Transform(location=unreal.Vector(*row['location_cm']),rotation=unreal.Rotator(yaw=row['yaw']),scale=unreal.Vector(1,1,1)),world_space=True)
assert helper['snapshot_actor_state'](untouched)==before and [native_state(c) for c in native]==native_before
result=runpy.run_path(str(root/'Scripts/Editor/check_z09_walls.py'))['check_z09_walls'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z09-walls-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=result,preserved_other_actors=len(untouched),preserved_native_components=native_before),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('entry',(-300,25750,-1330),(0,90),80),('wound',(300,28100,-1330),(0,90),80),('reverse',(350,30200,-1330),(0,-90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z09_wall_capture','exec'),globals())
