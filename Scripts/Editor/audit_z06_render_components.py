"""Settled inventory of every visible primitive intersecting breach rescue."""
import json,os,time
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
started=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(delta):
 if time.monotonic()-started<15:return
 unreal.unregister_slate_post_tick_callback(handle)
 try:
  rows=[]
  for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
   for c in a.get_components_by_class(unreal.PrimitiveComponent):
    if not c.get_editor_property('visible'):continue
    o,e,_=unreal.SystemLibrary.get_component_bounds(c)
    if not (-1800<=o.x+e.x and o.x-e.x<=1800 and 5900<=o.y+e.y and o.y-e.y<=11700 and -1000<=o.z+e.z and o.z-e.z<=1500):continue
    rows.append(dict(actor=a.get_actor_label(),actor_class=a.get_class().get_name(),component=c.get_name(),component_class=c.get_class().get_name(),transform=c.get_world_transform().export_text(),origin=[o.x,o.y,o.z],extent=[e.x,e.y,e.z],hidden=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_enabled()),mesh=c.static_mesh.get_path_name() if isinstance(c,unreal.StaticMeshComponent) and c.static_mesh else None))
  assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
  (out/'z06-render-components.json').write_text(json.dumps(dict(status='settled_read_only_inventory',components=rows),indent=2))
 finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
