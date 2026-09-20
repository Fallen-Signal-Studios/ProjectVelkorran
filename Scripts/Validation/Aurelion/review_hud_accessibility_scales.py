"""Actual HUD at supported UI scales after the weapon/radar integration fixture."""
import sys,time,json,os
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Scripts/Validation/Aurelion'))
import probe_hud_gameplay_fixture as fixture
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
report={'status':'running','samples':[],'qualification':'Controlled HUD scaling and text presentation, not mission progression.'}
scales=[1.0,1.5,2.0]; index=0; phase='fixture'; at=time.monotonic(); ended=None
# Optional operator gate allows an immersive viewport before measuring layout.
# The dimensions recorded below, not the capture filename, establish coverage.
require_immersive = os.environ.get('SOV_HUD_REQUIRE_IMMERSIVE') == '1'
started = time.monotonic()

def write(): (out/'hud-scale-review.json').write_text(json.dumps(report,indent=2))
def tick(dt):
 global index,phase,at,ended
 try:
  if time.monotonic()-started>300 and phase!='end':
   raise RuntimeError('HUD scale review exceeded its bounded deadline')
  if phase=='fixture':
   if fixture.report['status']=='running':return
   assert fixture.report['status']=='passed',str(fixture.report)
   phase='viewport' if require_immersive else 'apply'
  if phase=='viewport':
   if not (out/'viewport-ready.txt').exists():return
   world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
   pixels=unreal.WidgetLayoutLibrary.get_viewport_size(world)
   assert pixels.x>=1280 and pixels.y>=720, 'Viewport is below the requested minimum; do not qualify the editor window dimensions'
   phase='apply'
  if phase=='apply':
   if index==len(scales):
    report['status']='passed_requires_visual_review';phase='end';return
   settings=unreal.GameUserSettings.get_game_user_settings(); snapshot=settings.get_settings_snapshot()
   snapshot.set_editor_property('ui_scale',scales[index]);settings.apply_settings_snapshot(snapshot)
   world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
   pawn=unreal.GameplayStatics.get_player_pawn(world,0)
   for p in unreal.ObjectIterator(unreal.SovAccessibilityPresentation):
    if p.get_world()==world:
     p.present_speech(unreal.Text('Lyessa'),unreal.Text('Hold the relay. Keep the east stair clear while the survivors cross the terrace.'),5.0,pawn.get_actor_location(),True)
     p.present_caption(unreal.Text('Shield broken'),5.0,pawn.get_actor_location())
   phase='settle';at=time.monotonic();return
  if phase=='settle' and time.monotonic()-at>3:
   world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
   surfaces=[w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface) if w.is_in_viewport() and w.get_world()==world]
   assert len(surfaces)==1
   s=surfaces[0];view=s.get_holographic_hud_view();w=s.get_editor_property('AmmoText')
   pixels=unreal.WidgetLayoutLibrary.get_viewport_size(world)
   report['samples'].append({'scale_requested':scales[index],'scale_actual':view.scale,'ammo':str(w.get_text()),
    'viewport_pixels':[pixels.x,pixels.y], 'viewport_dpi_scale':unreal.WidgetLayoutLibrary.get_viewport_scale(world),
    'visible_clip':str(s.get_editor_property('AmmoClip').get_text()),
    'visible_reserve':str(s.get_editor_property('AmmoReserve').get_text())})
   unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/('hud-scale-'+str(scales[index])+'.png')))
   phase='capture';at=time.monotonic();write();return
  if phase=='capture' and time.monotonic()-at>3:index+=1;phase='apply';return
 except Exception as exc:report.update(status='failed',error=str(exc));phase='end'
 if phase=='end':
  if ended is None:
   unreal.GameUserSettings.get_game_user_settings().apply_settings_snapshot(fixture.original_settings)
   unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();ended=time.monotonic();write()
  elif time.monotonic()-ended>2:
   unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)



