"""Read original proxy collision immediately and after initial editor ticks."""
import json,os,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);baseline=json.loads((root/'Art/Source/Aurelion/AtriumCrownKit/placement-baseline.json').read_text())
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();labels={a.get_actor_label():a for a in actors}
def read():
 rows=[]
 for row in baseline['actors']:
  a=labels[row['actor']];c=a.static_mesh_component
  rows.append(dict(actor=row['actor'],actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),responses={name:str(c.get_collision_response_to_channel(unreal.CollisionChannel.cast(value))) for name,value in (('Pawn',2),('Visibility',3),('Camera',4))}))
 return rows
immediate=read();start=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(delta):
 if time.monotonic()-start<15:return
 unreal.unregister_slate_post_tick_callback(handle)
 try:
  assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
  (out/'proxy-collision-diagnostic.json').write_text(json.dumps(dict(immediate=immediate,settled=read(),status='measured_without_edits'),indent=2))
 finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
