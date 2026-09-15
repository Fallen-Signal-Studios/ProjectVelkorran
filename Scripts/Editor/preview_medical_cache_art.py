"""Replace only the cache visual; optionally save after static and access checks."""
from pathlib import Path
import json,os,runpy,shutil,time,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/MedicalCacheKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());old=json.loads((source/'cache-baseline.json').read_text())[0];a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.visual
assert c.static_mesh.get_path_name()==old['mesh'] and c.get_world_transform().export_text()==old['visual_transform']
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
assert not json.loads((source/'coplanar-faces.json').read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';spec=json.loads((source/'manifest.json').read_text())['modules'][0];materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()}
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if globals().get('PERSIST_MEDICAL_CACHE',False) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
a.modify();c.modify();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[]);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
a.label.modify();a.label.set_text('MEDICAL AID')
c.set_world_transform(unreal.Transform(location=unreal.Vector(-3050,22600,-1200),rotation=unreal.Rotator(yaw=180),scale=unreal.Vector(1,1,1)),False,False)
after=helpers['snapshot_actor_state'](actors);assert {k:v for k,v in before.items() if k!=a.get_path_name()}=={k:v for k,v in after.items() if k!=a.get_path_name()}
result=runpy.run_path(str(root/'Scripts/Editor/check_medical_cache_art.py'))['check_medical_cache_art'](actors)
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def finish_cache(delta):
    if time.monotonic()-started<15:return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
        if time.monotonic()-started<240:return
        unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise RuntimeError('Navigation did not settle')
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        result['access']=runpy.run_path(str(root/'Scripts/Editor/check_z08_refuge.py'))['check_refuge_access'](actors)
        persist=bool(globals().get('PERSIST_MEDICAL_CACHE',False))
        if persist:
            shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
        (out/'medical-cache-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_other_actor_states=3139),indent=2))
        code=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
        views="views=[('cache-front',(-3050,22360,-1070),(-15,90),70),('cache-quarter',(-2890,22400,-1050),(-20,130),75),('cache-context',(-2800,22380,-990),(-18,140),90)]\n"
        exec(compile(prefix+views+'state=dict'+suffix,'cache_views','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
handle=unreal.register_slate_post_tick_callback(finish_cache)
