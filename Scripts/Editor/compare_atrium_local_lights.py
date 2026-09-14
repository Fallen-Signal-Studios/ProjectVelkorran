"""Unsaved same-camera comparison of the four atrium bridge lights."""
import json,os,time
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);labels={a.get_actor_label():a for a in subsystem.get_all_level_actors()}
lights=[labels[f'ENVL_Z05_Key_{i:02}'].get_component_by_class(unreal.RectLightComponent) for i in range(4)]
before=[c.get_editor_property('intensity') for c in lights];assert before==[1190]*4
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-3100,-100,175),unreal.Rotator(pitch=16,yaw=0));camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(90)
editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
state=dict(phase=0,start=time.monotonic(),busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
 for c,value in zip(lights,before):c.set_intensity(value)
def tick(delta):
 if state['busy']:return
 state['busy']=True
 try:
  if state.get('task') and not state['task'].is_task_done():return
  elapsed=time.monotonic()-state['start']
  if state['phase']==0 and elapsed>25:
   state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/'local-lights-on.png'),camera);state['phase']=1
  elif state['phase']==1:
   for c in lights:c.set_intensity(0)
   state['phase']=2;state['start']=time.monotonic()
  elif state['phase']==2 and elapsed>25:
   state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/'local-lights-off.png'),camera);state['phase']=3
  elif state['phase']==3:
   restore();assert [c.get_editor_property('intensity') for c in lights]==before
   assert all((out/file).exists() for file in ('local-lights-on.png','local-lights-off.png'))
   (out/'comparison.json').write_text(json.dumps(dict(status='captured',lights=[c.get_owner().get_actor_label() for c in lights],before=before,comparison_intensity=0,restored=True,scope='Unsaved lighting experiment; camera fixed; no assets or map saved; visual interpretation required'),indent=2))
   unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
 except Exception:
  restore();unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
 finally:state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
