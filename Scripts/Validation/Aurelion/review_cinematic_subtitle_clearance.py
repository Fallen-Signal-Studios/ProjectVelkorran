"""Controlled native subtitle visual review; no mission progression or asset writes."""
import json
import os
import time
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
settings = unreal.GameUserSettings.get_game_user_settings()
original = settings.get_settings_snapshot()
settings.complete_accessibility_setup()
report = dict(status='running', samples=[], qualification='Controlled cinematic speech and decorative overlay; not a mission scene completion.')
phase = 'start'
started = time.monotonic()
at = started
index = 0
scales = [1., 1.5, 2.]
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def write():
    (out / 'cinematic-subtitle-clearance.json').write_text(json.dumps(report, indent=2))

def tick(delta):
    global phase, at, index
    try:
        if time.monotonic() - started > 120:
            raise AssertionError('Visual fixture timed out: ' + phase)
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase == 'start':
            editor.editor_request_begin_play()
            phase = 'ready'
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn or not pawn.is_character_ready() or pawn.is_character_pending_load():
            return
        if phase == 'ready':
            overlays = [w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
                        if w.get_name() == 'WBP_CinematicOverlay']
            if not overlays:
                return
            assert len(overlays) == 1
            overlay = overlays[0]
            assert overlay.get_render_opacity() == 0., 'Fresh HUD restored opaque decorative bars'
            overlay.call_method('Drop Black Bars')
            snapshot = settings.get_settings_snapshot()
            snapshot.set_editor_property('ui_scale', scales[index])
            settings.apply_settings_snapshot(snapshot)
            presentations = [p for p in unreal.ObjectIterator(unreal.SovAccessibilityPresentation) if p.get_world() == world]
            assert len(presentations) == 1
            presentations[0].present_speech(unreal.Text('Lyessa'), unreal.Text('Two mixed groups need passage: stretchers west, walkers east. Choose who moves first.'), 15., pawn.get_actor_location(), True)
            report['samples'].append(dict(scale=scales[index], overlay_opacity=overlay.get_render_opacity()))
            phase = 'capture'
            at = time.monotonic()
        elif phase == 'capture' and time.monotonic() - at > 4:
            unreal.SystemLibrary.execute_console_command(world, 'Shot showui -nosuffix filename=' + str(out / ('cinematic-subtitle-' + str(scales[index]) + '.png')))
            phase = 'advance'
            at = time.monotonic()
        elif phase == 'advance' and time.monotonic() - at > 3:
            index += 1
            if index < len(scales):
                phase = 'ready'
            else:
                report['status'] = 'passed_requires_visual_review'
                phase = 'end'
    except Exception as exc:
        report.update(status='failed', error=str(exc))
        phase = 'end'
    if phase == 'end':
        settings.apply_settings_snapshot(original)
        write()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)

handle = unreal.register_slate_post_tick_callback(tick)
