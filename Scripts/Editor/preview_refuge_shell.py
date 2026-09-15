"""Replace the measured refuge modules; save only after physical checks."""
from pathlib import Path
import json,os,runpy,shutil,time,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/RefugeShellKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors);by_label={a.get_actor_label():a for a in actors};baseline=json.loads((source/'shell-baseline.json').read_text());selected={r['path'] for r in baseline}
assert json.loads((source/'coverage-transfer.json').read_text())['status']=='passed'
for spec in json.loads((source/'manifest.json').read_text())['modules']:assert not json.loads((source/('coplanar-'+spec['asset']+'.json')).read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()};meshes={}
for spec in json.loads((source/'manifest.json').read_text())['modules']:
    path=dest+'/Meshes/'+spec['asset'];meshes[spec['asset']]=unreal.load_asset(path) if globals().get('PERSIST_REFUGE_SHELL',False) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
wall_source=root/'Art/Source/Aurelion/Z08WallKit'
wall_spec=next(r for r in json.loads((wall_source/'manifest.json').read_text())['modules'] if r['asset']=='SM_Aurelion_KIT_Z08WallAssembly')
if not globals().get('PERSIST_REFUGE_SHELL',False):helpers['import_owned_mesh'](wall_spec,wall_source,dest+'/Meshes',materials)
checker=runpy.run_path(str(root/'Scripts/Editor/check_refuge_shell.py'))
overlay=json.loads((source/'threshold-overlay-baseline.json').read_text());guide=by_label[overlay['actor']];gc=guide.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert guide.get_path_name()==overlay['path'] and gc.get_path_name()==overlay['component'] and gc.static_mesh.get_path_name()==overlay['mesh']
assert [gc.get_instance_transform(i,world_space=True).export_text() for i in range(gc.get_instance_count())]==overlay['all_transforms']
kept=[gc.get_instance_transform(i,world_space=True) for i in (2,3)];guide.modify();gc.modify();gc.clear_instances()
for t in kept:gc.add_instance(t,world_space=True)
selected.add(guide.get_path_name())
for row in baseline:
    a=by_label[row['actor']];c=a.get_component_by_class(unreal.StaticMeshComponent)
    assert a.get_path_name()==row['path'] and c.get_path_name()==row['meshes'][0]['path']
    assert a.get_actor_transform().export_text()==row['transform'] and c.get_world_transform().export_text()==row['meshes'][0]['transform'] and c.static_mesh.get_path_name()==row['meshes'][0]['mesh']
    assert a.get_actor_enable_collision()==row['collision'] and str(c.get_collision_enabled())==row['meshes'][0]['collision']
    name,length,location,yaw,threshold=checker['shell_spec'](row)
    a.modify();c.modify();c.set_static_mesh(meshes[name]);c.set_editor_property('override_materials',[])
    unreal.log('REFUGE_SHELL_PRIOR_VISIBILITY '+row['actor']+' '+str((a.get_editor_property('hidden'),c.get_editor_property('visible'),c.get_editor_property('hidden_in_game'))))
    a.set_actor_hidden_in_game(False);c.set_visibility(True,False);c.set_hidden_in_game(False,False)
    a.set_actor_transform(unreal.Transform(location=unreal.Vector(*location),rotation=unreal.Rotator(yaw=yaw),scale=unreal.Vector(1,1,1)),False,False)
    c.set_collision_profile_name('NoCollision' if threshold else 'BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION if threshold else unreal.CollisionEnabled.QUERY_AND_PHYSICS);a.set_actor_enable_collision(row['collision'])

after=helpers['snapshot_actor_state'](actors);assert {k:v for k,v in before.items() if k not in selected}=={k:v for k,v in after.items() if k not in selected}
check=runpy.run_path(str(root/'Scripts/Editor/check_z08_refuge.py'));result=checker['check_refuge_shell'](actors);result['retained_panels']=check['check_z08_refuge'](actors)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();unreal.SovAurelionNavigationLibrary.build_navigation(world,unreal.Vector(0,13000,-500),unreal.Vector(14000,38000,3000));started=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish_refuge(delta):
    if time.monotonic()-started<15:return
    if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
        if time.monotonic()-started>=240:
            unreal.unregister_slate_post_tick_callback(nav_handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise RuntimeError('Navigation timeout')
        return
    unreal.unregister_slate_post_tick_callback(nav_handle)
    try:
        result['access']=check['check_refuge_access'](actors);persist=bool(globals().get('PERSIST_REFUGE_SHELL',False))
        if persist:
            shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
        (out/'refuge-shell-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_other_actor_states=len(actors)-len(selected)),indent=2))
        exec(compile((root/'Scripts/Editor/review_refuge_shell.py').read_text(),'refuge_capture','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
nav_handle=unreal.register_slate_post_tick_callback(finish_refuge)
