"""Unsaved fixed-camera test of high-resolution capture temporal run-up."""
import json,os,time
from pathlib import Path
import unreal

out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cvar='r.HighResScreenshotDelay'
before=unreal.SystemLibrary.get_console_variable_int_value(cvar)
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-3100,-100,175),unreal.Rotator(pitch=16,yaw=0))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(90)
editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
state=dict(phase=0,start=time.monotonic(),busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def set_delay(value):
 unreal.SystemLibrary.execute_console_command(world,f'{cvar} {value}')
 assert unreal.SystemLibrary.get_console_variable_int_value(cvar)==value

def tick(delta):
 if state['busy']:return
 state['busy']=True
 try:
  if state.get('task') and not state['task'].is_task_done():return
  if time.monotonic()-state['start']<25:return
  if state['phase']<2:
   delay=(4,64)[state['phase']];set_delay(delay)
   state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/f'capture-delay-{delay}.png'),camera)
   state['phase']+=1;state['start']=time.monotonic()
  else:
   set_delay(before)
   assert all((out/f'capture-delay-{n}.png').exists() for n in (4,64))
   (out/'capture-history-comparison.json').write_text(json.dumps(dict(status='captured',delays=[4,64],original_delay=before,restored=True,scope='Unsaved fixed-camera capture diagnostic; no scene or render configuration saved; visual interpretation required'),indent=2))
   unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
 except Exception:
  set_delay(before);unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
 finally:state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
