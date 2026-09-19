"""45-second transient HUD contact presentation probe in PIE.
Synthetic contacts only: does not verify enemy acquisition or Blackout filtering.
Run after setup has closed, in an isolated validation profile. Do not stop PIE
before completion; cleanup restores the original view and frontend tick state.
"""
import unreal, time, json, os
from pathlib import Path
radar_out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
radar_surface=next(w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface) if w.is_in_viewport())
radar_original=radar_surface.get_holographic_hud_view()
radar_frontend=radar_surface.get_owning_player().get_component_by_class(unreal.SovFrontendComponent)
radar_original_tick=radar_frontend.is_component_tick_enabled()
radar_contacts=[]
for x,y,alpha,live in [(-45,-35,1.0,True),(45,-35,.55,False)]:
    contact=unreal.SovHolographicHUDContact()
    for name,value in [('Offset',unreal.Vector2D(x,y)),('Alpha',alpha),('bLiveSighting',live)]:
        contact.set_editor_property(name,value)
    radar_contacts.append(contact)

def radar_finish(error=None):
    try:
        radar_surface.set_editor_property('View',radar_original)
    finally:
        try:
            radar_frontend.set_component_tick_enabled(radar_original_tick)
        finally:
            unreal.unregister_slate_post_tick_callback(radar_handle)
    (radar_out/'radar-presentation-probe.json').write_text(json.dumps(dict(
        status='failed' if error else 'completed',error=error,
        scope='Synthetic view injection. Visual inspection required. Not gameplay detection or Blackout coverage.'),indent=2))

def radar_tick(delta):
    try:
        elapsed=time.monotonic()-radar_start
        if elapsed>45:
            radar_finish()
            return
        view=radar_surface.get_holographic_hud_view()
        view.set_editor_property('Contacts',radar_contacts if elapsed<30 else [])
        radar_surface.set_editor_property('View',view)
    except Exception as exc:
        radar_finish(str(exc))
        raise
radar_start=time.monotonic()
radar_handle=unreal.register_slate_post_tick_callback(radar_tick)
radar_frontend.set_component_tick_enabled(False)
