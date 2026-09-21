"""Read back saved departure modules after an earned CP9 load, then capture PIE.

Only the inspection camera is temporary. No walls, lights or characters are edited.
"""
import hashlib,json,runpy,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
saved=json.loads((root/'Saved/Validation/Aurelion/DepartureCofferSaved-20260920-225020-8248eef7/departure-wall-fit.json').read_text())
assert saved['status']=='saved_reloaded'
for asset,digest in saved['mesh_hashes'].items():
    assert hashlib.sha256((root/('Content/'+asset.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest()==digest
baseline=json.loads((root/'Art/Source/Aurelion/Z12DepartureWalls/wall-baseline.json').read_text())
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_departure_keys.py'))
scope=runpy.run_path(str(Path(__file__).with_name('check_earned_m13_reload_roundtrip.py')),init_globals={'M13_RELOAD_COUNT':1})
scope=scope['tick'].__globals__;original_finish=scope['finish'];state=dict(active=False,index=0)
views=[('player',None,None),('selene-background',(1350,48200,170),-90),('departure-side',(1000,47700,170),0)]

def cleanup():
    if not state.get('active'):return
    state['pc'].set_view_target_with_blend(state['view'],0)
    if state.get('camera'):state['camera'].destroy_actor()
    state['active']=False;scope['report']['departure_wall_review']['temporary_camera_removed']=True

def finish(error=None):
    if error:cleanup();original_finish(error);return
    scope['state']['phase']='stopping'
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
    owner=next(a for a in actors if a.get_actor_label()==baseline['actor']);actual={}
    for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent):
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        actual[c.static_mesh.get_path_name().split('.')[0]]=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
    assert actual==saved['expected']
    for row in baseline['native_walls']:
        actor=next(a for a in actors if a.get_actor_label()==row['actor'])
        assert actor.get_actor_transform().export_text()==row['transform'] and actor.get_actor_enable_collision()==row['collision']
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    state.update(active=True,pc=pc,view=pc.get_view_target(),world=world,at=time.monotonic())
    scope['report']['departure_wall_review']=dict(scope=__doc__,saved_modules=actual,frames=[],native_wall_poses_preserved=True,temporary_camera_removed=False)
    state['camera']=helper['summon'](world,'CameraActor')
    state['camera'].get_component_by_class(unreal.CameraComponent).set_field_of_view(80)
    scope['write']()
scope['finish']=finish

def tick(dt):
    if not state['active']:
        if scope['state']['phase']=='stopping':unreal.unregister_slate_post_tick_callback(handle)
        return
    try:
        now=time.monotonic()
        if now-state['at']<12:return
        name,position,yaw=views[state['index']];review=scope['report']['departure_wall_review']
        if not state.get('shot'):
            manager=unreal.GameplayStatics.get_player_camera_manager(state['world'],0)
            if position:assert (manager.get_camera_location()-unreal.Vector(*position)).length()<1
            file=name+'.png'
            unreal.SystemLibrary.execute_console_command(state['world'],'Shot showui -nosuffix filename='+str(scope['out']/file))
            review['frames'].append(dict(file=file,camera=manager.get_camera_location().export_text()))
            state['shot']=now;scope['write']();return
        if now-state['shot']<2:return
        assert (scope['out']/review['frames'][-1]['file']).exists()
        state['index']+=1
        if state['index']==len(views):
            cleanup();original_finish();unreal.unregister_slate_post_tick_callback(handle);return
        name,position,yaw=views[state['index']]
        state['camera'].set_actor_location(unreal.Vector(*position),False,False)
        state['camera'].set_actor_rotation(unreal.Rotator(yaw=yaw),False)
        state['pc'].set_view_target_with_blend(state['camera'],0);state.update(at=now,shot=None)
    except Exception:cleanup();original_finish(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
