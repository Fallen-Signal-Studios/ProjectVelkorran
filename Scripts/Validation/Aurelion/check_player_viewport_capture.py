"""Capture the local player's scene and Slate HUD at a fresh ready M12 start."""
import json
import os
import time
import traceback
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
report = dict(status='running', scope=__doc__)
state = dict(start=time.monotonic(), phase='starting', stopped_at=0., busy=False)


def write():
    (out / 'player-viewport-capture.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def tick(_delta):
    if state['busy']:
        return
    state['busy'] = True
    try:
        now = time.monotonic()
        if state['phase'] == 'stopping':
            if not level.is_in_play_in_editor() or now-state['stopped_at'] > 15.:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now-state['start'] < 150., 'PIE viewport capture deadline'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
        if not isinstance(pawn, unreal.SovTarrikCharacter) or not pawn.is_character_ready():
            return
        capture = unreal.SovAurelionPIEInputLibrary.capture_aurelion_pie_viewport(
            world, str(out / 'm12-player-viewport.png'))
        report['capture'] = capture.export_text()
        with_ui = unreal.SovAurelionPIEInputLibrary.capture_aurelion_pie_viewport_with_ui(
            world, str(out / 'm12-player-viewport-with-ui.png'))
        report['capture_with_ui'] = with_ui.export_text()
        report['status'] = 'passed' if capture.captured and with_ui.captured else 'failed'
        write()
        level.editor_request_end_play()
        state.update(phase='stopping', stopped_at=now)
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        write()
        level.editor_request_end_play()
        state.update(phase='stopping', stopped_at=time.monotonic())
    finally:
        state['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
write()
level.editor_request_begin_play()
