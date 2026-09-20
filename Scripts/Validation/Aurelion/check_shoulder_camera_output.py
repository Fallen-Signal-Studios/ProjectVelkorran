"""Controlled rendered PIE check of actual left/right camera output; not input or mission acceptance."""
import unreal,os,json,time,math,traceback
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not (out/'shoulder-runtime.json').exists(), 'Preserve previous camera evidence'
report=dict(status='running',scope=__doc__,samples=[])
started=time.monotonic(); stage_at=started; claim=None; pawn=None; component=None; index=-1; ending=False
cases=[(s,h) for s in ('FAR','BALANCED','CLOSE') for h in ('RIGHT','LEFT')]
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def write(): (out/'shoulder-runtime.json').write_text(json.dumps(report,indent=2))
def finish(error=None):
 global ending,stage_at,claim
 if claim and component: component.release_camera(claim); claim=None
 report['status']='failed' if error else 'passed'
 if error:report['error']=error
 write();ending=True;stage_at=time.monotonic();editor.editor_request_end_play()
def tick(dt):
 global pawn,component,claim,index,stage_at
 try:
  now=time.monotonic()
  if ending:
   if not editor.is_in_play_in_editor() or now-stage_at>10:
    unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
   return
  assert now-started<150,'PIE camera check timed out'
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not world:return
  active=unreal.GameplayStatics.get_player_pawn(world,0)
  if not isinstance(active,unreal.SovPlayerCharacterBase) or not active.is_character_ready():return
  if pawn is None:
   expected=globals().get('EXPECTED_PAWN_CLASS')
   assert not expected or active.get_class().get_name()==expected, active.get_class().get_name()
   pawn=active;component=pawn.get_component_by_class(unreal.SovCameraControlComponent)
   cam=pawn.get_component_by_class(unreal.GameplayCameraComponent)
   reference=cam.get_editor_property('CameraReference').export_text()
   assert 'CA_SovProtagonist' in reference,reference
   report['pawn']=pawn.get_class().get_name();report['camera']=reference
   stage_at=now
   index=0
   s,h=cases[index]
   claim=component.request_camera(unreal.SovCameraRequest(priority=unreal.SovCameraPriority.CINEMATIC,style=getattr(unreal.SovCameraStyle,s),mode=unreal.SovCameraMode.STRAFE,shoulder=getattr(unreal.SovCameraShoulder,h),reason='ControlledShoulderOutput'))
   return
  assert active==pawn,'Unexpected possession during camera check'
  if now-stage_at<3:return
  manager=unreal.GameplayStatics.get_player_camera_manager(world,0)
  location=manager.get_camera_location();rotation=manager.get_camera_rotation();origin=pawn.get_actor_location()
  yaw=math.radians(rotation.yaw);dx=location.x-origin.x;dy=location.y-origin.y
  side=-math.sin(yaw)*dx+math.cos(yaw)*dy
  s,h=cases[index]
  row=dict(style=s,shoulder=h,resolved=component.get_camera_state().export_text(),camera=[location.x,location.y,location.z],origin=[origin.x,origin.y,origin.z],yaw=rotation.yaw,lateral=side,fov=manager.get_fov_angle(),supplied=pawn.call_method('Get_CharacterPropertiesForCamera').export_text())
  report['samples'].append(row);write()
  assert math.isfinite(side) and (side>10 if h=='RIGHT' else side < -10),row
  component.release_camera(claim);claim=None;index+=1
  if index==len(cases):finish();return
  s,h=cases[index]
  claim=component.request_camera(unreal.SovCameraRequest(priority=unreal.SovCameraPriority.CINEMATIC,style=getattr(unreal.SovCameraStyle,s),mode=unreal.SovCameraMode.STRAFE,shoulder=getattr(unreal.SovCameraShoulder,h),reason='ControlledShoulderOutput'))
  stage_at=now
 except Exception:finish(traceback.format_exc())
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
attached=globals().get('ATTACH_TO_EXISTING_PIE',False)
if attached:
 assert editor.is_in_play_in_editor()
else:
 settings=unreal.SovGameUserSettings.get_game_user_settings()
 assert settings.complete_accessibility_setup()
 assert editor.load_level('/Game/Aurelion/Maps/L_Aurelion_M12')
handle=unreal.register_slate_post_tick_callback(tick)
write()
if not attached:editor.editor_request_begin_play()
