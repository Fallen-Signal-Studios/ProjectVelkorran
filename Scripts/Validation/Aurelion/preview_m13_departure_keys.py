"""Compare local departure keys after an earned public CP9 reload; save no assets."""
import json,runpy,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
keys=runpy.run_path(str(root/'Scripts/Editor/aurelion_departure_keys.py'))
saved_review=globals().get('VERIFY_SAVED_KEYS')
scope=runpy.run_path(str(Path(__file__).with_name('check_earned_m13_reload_roundtrip.py')),init_globals={'M13_RELOAD_COUNT':1})
scope=scope['tick'].__globals__
original_finish=scope['finish'];state=dict(active=False,stage=0)
cases=([(hero,keys['INTENSITIES'][hero]) for hero in ('Tarrik','Selene')] if saved_review
    else [(hero,power) for hero in ('Tarrik','Selene') for power in (0.,240.,600.)])
def cleanup():
    if not state.get('active'):return
    pc=state['pc'];pc.set_view_target_with_blend(state['view'],0)
    if not saved_review:
        for a in state.get('lights',[]):a.destroy_actor()
    if state.get('camera'):state['camera'].destroy_actor()
    state['active']=False
    scope['report']['departure_key_review']['temporary_actors_removed']=True
def finish(error=None):
    if error:cleanup();original_finish(error);return
    # The completed reload checker idles here while this observer compares light only.
    scope['state']['phase']='stopping'
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    if saved_review:
        saved=json.loads(Path(saved_review).read_text());assert saved['status']=='saved_reloaded'
        lights=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RectLight) if a.get_actor_label().startswith(keys['PREFIX'])]
        assert len(lights)==2
        assert sorted([keys['describe'](a) for a in lights],key=lambda v:v['label'])==sorted(saved['lights'],key=lambda v:v['label'])
    else:lights=keys['spawn_preview'](world)
    state.update(active=True,pc=pc,view=pc.get_view_target(),lights=lights,at=time.monotonic(),world=world)
    camera=keys['summon'](world,'CameraActor')
    camera.set_actor_location(unreal.Vector(1530,48550,210),False,False)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(1350,48000,115)),False)
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(50.)
    state['camera']=camera
    scope['report']['departure_key_review']=dict(scope=__doc__,saved_light_readback=bool(saved_review),frames=[],temporary_actors_removed=False)
    scope['write']()
scope['finish']=finish
def tick(dt):
    if not state['active']:
        if scope['state']['phase']=='stopping':unreal.unregister_slate_post_tick_callback(handle)
        return
    try:
        now=time.monotonic();hero,power=cases[state['stage']]
        if now-state['at']<8:return
        review=scope['report']['departure_key_review']
        if not state.get('shot'):
            name=hero.lower()+'-'+str(int(power))+'.png'
            review['frames'].append(dict(file=name,hero=hero,intensity=power,lights=[keys['describe'](a) for a in state['lights']]))
            unreal.SystemLibrary.execute_console_command(state['world'],'Shot showui -nosuffix filename='+str(scope['out']/name))
            state['shot']=now;scope['write']();return
        if now-state['shot']<2:return
        assert (scope['out']/review['frames'][-1]['file']).exists()
        state['stage']+=1
        if state['stage']==len(cases):
            cleanup();original_finish();unreal.unregister_slate_post_tick_callback(handle);return
        hero,power=cases[state['stage']]
        if not saved_review:
            for a in state['lights']:a.get_component_by_class(unreal.RectLightComponent).set_intensity(power)
        state['pc'].set_view_target_with_blend(state['view'] if hero=='Tarrik' else state['camera'],0)
        state.update(at=now,shot=None)
    except Exception:cleanup();original_finish(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
