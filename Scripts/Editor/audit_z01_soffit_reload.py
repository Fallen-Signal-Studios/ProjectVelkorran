"""Read-only immediate and settled collision diagnostics for the retained Z01 soffit."""
import json,os,time
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors}
physical=[a for a in actors if a.get_actor_label().startswith(('Z01_Wall','Z01_Lintel')) or a.get_actor_label()=='aureliondoors'];ignored=[a for a in actors if a not in physical]
rows=[];states=[]
for a in physical:
 states.append(dict(actor=a.get_actor_label(),transform=a.get_actor_transform().export_text(),collision=a.get_actor_enable_collision(),components=[dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,transform=c.get_world_transform().export_text(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name())) for c in a.get_components_by_class(unreal.StaticMeshComponent)]))
def sample(phase):
 for y in (-17500,-11900):
  for x in (-7252,-7000,-6748):
   for z in (326,356,358,360):
    hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,y-300,z),unreal.Vector(x,y+300,z),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
    t=hit.to_tuple() if hit else None
    rows.append(dict(phase=phase,x=x,y=y,z=z,hit=dict(actor=t[9].get_actor_label(),point=t[5].export_text()) if t and t[0] else None))
 (out/'soffit-diagnostics.json').write_text(json.dumps(dict(states=states,queries=rows),indent=2))
sample('immediate');unreal.EditorPythonScripting.set_keep_python_script_alive(True);start=time.monotonic()
def tick(delta):
 if time.monotonic()-start<15:return
 unreal.unregister_slate_post_tick_callback(handle)
 try:
  sample('settled');assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
 finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
