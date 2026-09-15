"""Fit modular sensor-gallery walls on the original art actor."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir()); source=root/'Art/Source/Aurelion/Z03WallKit'; out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
old=json.loads((source/'wall-baseline.json').read_text()); manifest=json.loads((source/'manifest.json').read_text())
a=next(a for a in actors if a.get_actor_label()==old['actor']); original=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert original.static_mesh.get_path_name()==old['mesh']
assert [original.get_instance_transform(i,world_space=True).export_text() for i in range(original.get_instance_count())]==old['all_instance_transforms']
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
untouched=[actor for actor in actors if actor!=a]; before=helper['snapshot_actor_state'](untouched)
dest='/Game/Aurelion/Environment/ArchitectureKit'; persist=bool(globals().get('PERSIST_Z03_WALLS',False))
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem); lib=unreal.SubobjectDataBlueprintFunctionLibrary
handles=sub.k2_gather_subobject_data_for_instance(a); parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
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
        sub.rename_subobject(h,suffix); c=lib.get_associated_object(lib.get_data(h))
    c.modify(); c.clear_instances(); c.set_static_mesh(mesh); c.set_editor_property('override_materials',[])
    c.set_collision_profile_name('NoCollision'); c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for row in manifest['placements']:
        if row['asset']==spec['asset']:
            c.add_instance(unreal.Transform(location=unreal.Vector(*row['location_cm']),rotation=unreal.Rotator(yaw=row['yaw']),scale=unreal.Vector(1,1,1)),world_space=True)
assert helper['snapshot_actor_state'](untouched)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z03_walls.py'))['check_z03_walls'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap'); assert editor.save_current_level()
(out/'z03-walls-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text(); prefix=capture.split('views=[',1)[0]; suffix=capture.split('state=dict',1)[1]
views="views=[('approach',(6800,-19300,165),(0,90),90),('middle',(6800,-17200,165),(15,90),90),('wall-detail',(6900,-17800,180),(10,170),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z03_wall_review','exec'),globals())
