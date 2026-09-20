"""Observe real route firearm instances; controlled camera checks restore preferences.
No weapon equip, damage, mission or actor-placement writes are made here.
"""
import json,os,time,traceback
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'firearm-route.json'
report=dict(status='observing',checks={},samples=[],errors=[])
started=time.monotonic();last=0.;seen=False

def instances(pawn):
 return [a for a in unreal.ObjectIterator(unreal.NarrativeAnimInstance)
  if a.get_class().get_path_name().startswith('/Game/Characters/Animation/Firearms/')
  and isinstance(a.get_outer(),unreal.SkeletalMeshComponent)
  and 'UEDPIE_' in a.get_path_name() and a.get_character_ref()==pawn]

def write():out.write_text(json.dumps(report,indent=2))
def tick(delta):
 global last,seen
 now=time.monotonic()
 if now-last<.5:return
 last=now
 try:
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if (seen and not world) or now-started>1800:
   report['status']='finished';unreal.unregister_slate_post_tick_callback(handle);write();return
  if not world:return
  seen=True;pawn=unreal.GameplayStatics.get_player_pawn(world,0)
  if not pawn or not pawn.is_character_ready() or pawn.is_character_pending_load():return
  linked=instances(pawn)
  if not linked:return
  hero=pawn.get_class().get_name();component=pawn.get_component_by_class(unreal.SovCameraControlComponent)
  state=component.get_camera_state()
  report['samples'].append(dict(elapsed=round(now-started,2),hero=hero,style=str(state.style),
   weapon=str(pawn.get_weapon()),instances=[dict(path=a.get_path_name(),first=a.get_editor_property('ResolvedFirstPerson'),reload=a.get_editor_property('Reloading')) for a in linked]))
  if hero not in report['checks'] and state.priority==unreal.SovCameraPriority.PROFILE:
   previous=pawn.get_editor_property('CameraStyle');rows=[];claim=None
   try:
    for mode in ('FAR','FIRST_PERSON','BALANCED'):
     pawn.call_method('SetCameraMode',(getattr(type(previous),mode),))
     first=component.get_camera_state().style==unreal.SovCameraStyle.FIRST_PERSON
     values=[a.get_editor_property('ResolvedFirstPerson') for a in linked]
     rows.append(dict(mode=mode,values=values));assert all(v==first for v in values)
    pawn.call_method('SetCameraMode',(getattr(type(previous),'FIRST_PERSON'),))
    claim=component.request_camera(unreal.SovCameraRequest(priority=unreal.SovCameraPriority.AIM,style=unreal.SovCameraStyle.CLOSE,mode=unreal.SovCameraMode.STRAFE,reason='ControlledFirearmRouteCheck'))
    assert all(not a.get_editor_property('ResolvedFirstPerson') for a in linked)
    assert component.release_camera(claim);claim=None
    assert all(a.get_editor_property('ResolvedFirstPerson') for a in linked)
    report['checks'][hero]=dict(status='passed',modes=rows,aim_override_and_restore=True)
   finally:
    if claim is not None:component.release_camera(claim)
    pawn.call_method('SetCameraMode',(previous,))
  write()
 except Exception:
  report['errors'].append(traceback.format_exc());report['status']='failed';write();unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
write()
