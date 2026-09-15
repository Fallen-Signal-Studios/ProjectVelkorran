"""Replace vendor paving on its original actor, preserving its walking datum."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09FloorKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
old=json.loads((source/'floor-baseline.json').read_text(encoding='utf-8-sig'));manifest=json.loads((source/'manifest.json').read_text())
a=next(a for a in actors if a.get_actor_label()==old['actor']);original=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert original.static_mesh.get_path_name()==old['mesh'] and [original.get_instance_transform(i,world_space=True).export_text() for i in range(original.get_instance_count())]==[r['transform'] for r in old['instances']]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state']([x for x in actors if x!=a])
geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))
dest='/Game/Aurelion/Environment/ArchitectureKit';persist=bool(globals().get('PERSIST_Z09_FLOOR',False))
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_Basalt':'PavingBasalt','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
parent=next(h for h in sub.k2_gather_subobject_data_for_instance(a) if lib.get_associated_object(lib.get_data(h))==a)
for index,spec in enumerate(manifest['modules']):
    assert not json.loads((source/(spec['asset'].replace('SM_Aurelion_KIT_','')+'-coplanar.json')).read_text())['overlaps']
    mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    assert mesh
    if index==0:c=original
    else:
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.HierarchicalInstancedStaticMeshComponent,conform_transform_to_parent=True));assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,'Z09PavingApproach');c=lib.get_associated_object(lib.get_data(h))
    c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[]);c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
    for r in old['instances'][:56] if index==0 else old['instances'][56:]:
        t=unreal.Transform(location=unreal.Vector(*r['location']),rotation=unreal.Quat(*r['quaternion']).rotator(),scale=unreal.Vector(*r['scale']))
        lo,hi=geo['bounds'](geo['corners'](t,old['mesh_origin'],old['mesh_extent']))
        c.add_instance(unreal.Transform(location=unreal.Vector((lo[0]+hi[0])/2,(lo[1]+hi[1])/2,hi[2]-12),rotation=unreal.Rotator(),scale=unreal.Vector(1,1,1)),world_space=True)
assert helper['snapshot_actor_state']([x for x in actors if x!=a])==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z09_floor.py'))['check_z09_floor'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z09-floor-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=result),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('entry',(-300,25750,-1330),(-8,90),80),('paving',(-300,26500,-1300),(-35,70),80),('approach',(0,24700,-1330),(-12,90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z09_floor_capture','exec'),globals())
