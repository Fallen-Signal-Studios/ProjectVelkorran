"""Replace the measured refuge modules; save only after physical checks."""
from pathlib import Path
import json,os,runpy,shutil,time,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z08RefugeKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors);by_label={a.get_actor_label():a for a in actors};baseline=json.loads((source/'refuge-baseline.json').read_text());selected={r['path'] for r in baseline}
for width in (1,2):assert not json.loads((source/('coplanar-'+str(width)+'m.json')).read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()};meshes={}
for spec in json.loads((source/'manifest.json').read_text())['modules']:
    path=dest+'/Meshes/'+spec['asset'];meshes[spec['asset']]=unreal.load_asset(path) if globals().get('PERSIST_Z08_REFUGE',False) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
for row in baseline:
    a=by_label[row['actor']];c=a.get_component_by_class(unreal.StaticMeshComponent)
    assert a.get_actor_transform().export_text()==row['actor_transform'] and c.get_world_transform().export_text()==row['component_transform'] and c.static_mesh.get_path_name()==row['mesh']
    width=1 if row['scale'][0]==.5 else 2;a.modify();c.modify();c.set_static_mesh(meshes['SM_Aurelion_KIT_RefugePanel_'+str(width)+'m']);c.set_editor_property('override_materials',[]);a.set_actor_scale3d(unreal.Vector(1,1,1))
    c.set_collision_profile_name('BlockAll' if row['actor_collision'] else 'NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if row['actor_collision'] else unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(row['actor_collision'])
after=helpers['snapshot_actor_state'](actors);assert {k:v for k,v in before.items() if k not in selected}=={k:v for k,v in after.items() if k not in selected}
check=runpy.run_path(str(root/'Scripts/Editor/check_z08_refuge.py'));result=check['check_z08_refuge'](actors)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();unreal.SovAurelionNavigationLibrary.build_navigation(world,unreal.Vector(0,13000,-500),unreal.Vector(14000,38000,3000));started=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish_refuge(delta):
    if time.monotonic()-started<15:return
    if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
        if time.monotonic()-started>=240:
            unreal.unregister_slate_post_tick_callback(nav_handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise RuntimeError('Navigation timeout')
        return
    unreal.unregister_slate_post_tick_callback(nav_handle)
    try:
        result['access']=check['check_refuge_access'](actors);persist=bool(globals().get('PERSIST_Z08_REFUGE',False))
        if persist:
            shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
        (out/'refuge-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_other_actor_states=len(actors)-len(selected)),indent=2))
        exec(compile((root/'Scripts/Editor/review_z08_refuge.py').read_text(),'refuge_capture','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
nav_handle=unreal.register_slate_post_tick_callback(finish_refuge)
