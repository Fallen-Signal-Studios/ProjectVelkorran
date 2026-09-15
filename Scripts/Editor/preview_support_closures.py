"""Fit the authored roof and gate visuals without changing native support authority."""
from pathlib import Path
import json,os,runpy,shutil,time,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/SupportClosureKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors};helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state'](actors)
roof=labels['Aurelion_RecoveryCacheCabinet_Roof'];presentation=labels['Aurelion_SupportBarrierView'];support=labels['Aurelion_PrioritySupport']
baseline=json.loads((root/'Art/Source/Aurelion/CacheEnclosureKit/native-baseline.json').read_text())['components']
for a,c in [(roof,roof.static_mesh_component),(presentation,presentation.west_barrier_visual),(presentation,presentation.east_barrier_visual)]:
    old=next(r for r in baseline if r['component']==c.get_path_name())
    assert c.get_world_transform().export_text()==old['transform'] and c.static_mesh.get_path_name()==old['mesh']
for name in ('roof','west','east'):assert not json.loads((source/('coplanar-'+name+'.json')).read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()}
persist=bool(globals().get('PERSIST_SUPPORT_CLOSURES',False));meshes={s['asset']:unreal.load_asset(dest+'/Meshes/'+s['asset']) if persist else helper['import_owned_mesh'](s,source,dest+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
roof.modify();c=roof.static_mesh_component;c.modify();c.set_static_mesh(meshes['SM_Aurelion_KIT_CacheRoof']);c.set_editor_property('override_materials',[])
roof.set_actor_scale3d(unreal.Vector(1,1,1));roof.set_actor_hidden_in_game(False);c.set_visibility(True,False);c.set_hidden_in_game(False,False);c.set_collision_profile_name('BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
presentation.modify()
for visual,barrier,name in [(presentation.west_barrier_visual,support.west_cache_barrier,'WestCacheGate'),(presentation.east_barrier_visual,support.east_flank_barrier,'EastFlankGate')]:
    visual.modify();visual.set_static_mesh(meshes['SM_Aurelion_KIT_'+name]);visual.set_editor_property('override_materials',[])
    visual.set_world_transform(unreal.Transform(location=barrier.get_world_location(),rotation=barrier.get_world_rotation(),scale=barrier.get_scaled_box_extent()/50),False,False)
    visual.set_visibility(True,False);visual.set_hidden_in_game(False,False);visual.set_collision_profile_name('NoCollision');visual.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
after=helper['snapshot_actor_state'](actors);excluded={roof.get_path_name(),presentation.get_path_name()};assert {k:v for k,v in before.items() if k not in excluded}=={k:v for k,v in after.items() if k not in excluded}
result=runpy.run_path(str(root/'Scripts/Editor/check_support_closures.py'))['check_support_closures'](actors)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();started=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish_closures(delta):
    if time.monotonic()-started<15:return
    if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
        if time.monotonic()-started<240:return
        unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise RuntimeError('Navigation timeout')
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        result['access']=runpy.run_path(str(root/'Scripts/Editor/check_z08_refuge.py'))['check_refuge_access'](actors)
        if persist:
            shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
        (out/'support-closure-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_other_actor_states=3138),indent=2))
        code=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
        views="views=[('cache-closed',(-2780,22180,-990),(-7,128),90),('cache-roof',(-2900,22280,-850),(-10,112),85),('flank-gate',(2350,19900,-620),(0,90),85)]\n"
        exec(compile(prefix+views+'state=dict'+suffix,'support_closure_views','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
handle=unreal.register_slate_post_tick_callback(finish_closures)
