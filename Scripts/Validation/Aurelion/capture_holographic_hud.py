"""Render the holographic combat HUD in a real session and capture it.

Every check so far proves the surface behaves; none proves it looks like anything. This starts an
ordinary M12 play session, waits for a ready protagonist and a valid HUD snapshot, captures the
rendered frame, and stops. It injects no input, writes no gameplay state and fabricates no progress.

Run visibly: an offscreen session has nothing to photograph.
"""
import json
import os
from pathlib import Path
import time
import unreal

# Parameterised so a second protagonist can be captured without editing this file.
MAP = os.environ.get('SOV_HUD_MAP', '/Game/Aurelion/Maps/L_Aurelion_M12')
MISSION = os.environ.get('SOV_HUD_MISSION', 'M12_FireAndFrost')
# The run has to prove which protagonist it photographed rather than assume it. M13 opens on Tarrik
# and only hands to Selene later, so pointing at her map is not the same as capturing her.
PROTAGONIST = os.environ.get('SOV_HUD_PROTAGONIST', 'Sov.Character.Player.Tarrik')
# Optional: a UI scale to capture at, and a subtitle plus caption on screen, to check text stays clear of the HUD.
UI_SCALE = os.environ.get('SOV_HUD_UI_SCALE')
WITH_TEXT = os.environ.get('SOV_HUD_WITH_TEXT') == '1'
# Optional: an authored surface to install before capturing, so the picture is of the widget rather
# than the painter. The frontend resolves this class every refresh, which is what makes a live swap work.
SURFACE = os.environ.get('SOV_HUD_SURFACE')
READY_SECONDS = 180.
SHOT_SECONDS = 45.
SETTLE_SECONDS = 1.5
STOP_SECONDS = 30.
WIDTH, HEIGHT = 2560, 1080

_RUN = None


class Run:
    def __init__(self, output_directory):
        self.out = Path(output_directory)
        self.out.mkdir(parents=True, exist_ok=True)
        self.started = time.monotonic()
        self.phase_at = self.started
        self.phase = 'begin'
        self.handle = None
        self.done = False
        self.shot = self.out / 'holographic-hud.png'
        # The UI-inclusive shutter. Kept separate so the two images from the same frame can be
        # compared: that comparison is the evidence about the instrument, not just about the HUD.
        self.ui_shot = self.out / 'holographic-hud-ui.png'
        self.report = dict(status='running', scope='Rendered holographic HUD in a real M12 session',
                           method='Ordinary play session; no input injection, no state writes',
                           physical_input_validation=False, capture=str(self.shot), observations=[])

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        temp = self.out / 'hud-capture.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        os.replace(temp, self.out / 'hud-capture.json')

    def stage(self, name):
        self.phase, self.phase_at = name, time.monotonic()
        self.report['observations'].append(dict(phase=name, elapsed=time.monotonic()-self.started))

    def finish(self, passed, reason):
        """Records the outcome. Teardown belongs to the stopped phase: unregistering the tick here
        would strand a running session, because stopping play needs further ticks."""
        if self.done:
            return
        self.done = True
        self.report['status'] = 'passed' if passed else 'failed'
        self.report['reason'] = reason
        # A material whose shader will not compile for Slate is replaced by the default material and
        # draws nothing, while everything else still reports success: the asset loads, its
        # parameters resolve, the surface paints. That failure was found only by looking at a
        # picture, so it is checked here instead.
        substitutions = []
        log = self.out / 'Editor.log'
        if log.exists():
            try:
                for line in log.read_text(encoding='utf8', errors='ignore').splitlines():
                    if 'Failed to compile Material' in line:
                        substitutions.append(line.strip())
            except OSError as error:
                self.report['material_log_unreadable'] = str(error)
        self.report['default_material_substitutions'] = substitutions
        if substitutions:
            # Written straight to the report: status and reason were already recorded above, so
            # reassigning the arguments here would leave a run reading 'failed' while still carrying
            # the success reason - a check that disagrees with itself and misdirects whoever reads it.
            self.report['status'] = 'failed'
            self.report['reason'] = 'A material fell back to the default material: ' + substitutions[0]
        self.report['capture_exists'] = self.shot.exists()
        if self.shot.exists():
            self.report['capture_bytes'] = self.shot.stat().st_size
        # Recorded independently: the UI-inclusive image is the one that can show the HUD at all, so
        # a run that produced only the other must not read as a success.
        self.report['ui_capture'] = str(self.ui_shot)
        self.report['ui_capture_exists'] = self.ui_shot.exists()
        if self.ui_shot.exists():
            self.report['ui_capture_bytes'] = self.ui_shot.stat().st_size
        self.write()
        unreal.log('AURELION_HUD_CAPTURE ' + self.report['status'] + ' ' + str(self.out / 'hud-capture.json'))

    def retire(self):
        """Releases the session so the editor can exit; without this the run burns its whole timeout."""
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)

    def player(self, world):
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        state = pc.get_campaign_state() if isinstance(pc, unreal.SovPlayerController) else None
        return pc, pawn, state

    def sample(self, world, key):
        """Diagnostics read at one named instant.

        Sampling only at first detection cannot tell a surface that painted once from one that
        paints every frame, and those are different defects with different fixes.
        """
        surfaces = [w for w in unreal.ObjectIterator(unreal.SovHolographicHUDWidget)
                    if w.get_world() == world]
        self.report['surface_' + key] = dict(
            instances=len(surfaces),
            visibility=[str(w.get_visibility()) for w in surfaces],
            diagnostics=[w.get_paint_diagnostics() for w in surfaces])
        self.write()
        return surfaces

    def tick(self, _delta):
        # Deliberately no early return once the outcome is recorded: the result is written before
        # teardown, and stopping play needs further ticks. Guarding on it here stranded a session in
        # play until its timeout expired, holding the build binaries open.
        try:
            editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            now = time.monotonic()
            if self.phase == 'begin':
                if editor.is_in_play_in_editor():
                    return
                editor.editor_request_begin_play()
                self.stage('await_ready')
                return
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if self.phase == 'await_ready':
                if now-self.phase_at > READY_SECONDS:
                    self.finish(False, 'The session never reached ' + PROTAGONIST + ' ready in ' + MISSION)
                    self.stage('end_play')
                    return
                if world is None or 'L_Aurelion_M12' not in world.get_name():
                    return
                pc, pawn, state = self.player(world)
                mission = state.get_active_mission() if state else None
                if pawn is None or mission is None or str(mission.mission_id) != MISSION:
                    return
                if not (pawn.is_character_ready() and pawn.is_alive()):
                    return
                if SURFACE and not self.report.get('surface_class'):
                    frontend = pc.get_frontend() if isinstance(pc, unreal.SovPlayerController) else None
                    surface_class = unreal.load_class(None, SURFACE)
                    assert frontend and surface_class, 'Could not install the authored HUD surface ' + SURFACE
                    frontend.set_holographic_hud_surface_class(surface_class)
                    self.report['surface_class'] = SURFACE
                    # The next refresh creates it; capture only once that has happened.
                    return
                if SURFACE and not [w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface)
                                    if w.get_world() == world]:
                    return
                # The surface must actually be up before its picture means anything. Its snapshot
                # reader is ordinary C++ with no reflection, so this observes the live widget instead.
                surfaces = [w for w in unreal.ObjectIterator(unreal.SovHolographicHUDWidget)
                            if w.get_world() == world]
                shown = [w for w in surfaces
                         if w.get_visibility() != unreal.SlateVisibility.COLLAPSED]
                if not shown:
                    return
                # The surface reports the live protagonist tag in its diagnostics. Requiring it here
                # means a run that never reaches the intended protagonist fails loudly, instead of
                # quietly photographing the other one and reporting success.
                if not any(PROTAGONIST in w.get_paint_diagnostics() for w in shown):
                    return
                self.report['protagonist'] = PROTAGONIST
                self.sample(world, 'at_detect')
                # The paint probe stays off. It proved every submission route draws correctly, and
                # the blank captures were the screenshot path excluding UI rather than anything in
                # the surface; leaving it on would only cover the HUD in diagnostic rectangles.
                # Re-enable with 'sov.HUD.HolographicPaintProbe 1' if a drawing route is ever suspect.
                self.report['paint_probe'] = 'disabled'
                self.stage('settle')
                return
            if self.phase == 'settle':
                if WITH_TEXT and not self.report.get('text_presented'):
                    surfaces = [w for w in unreal.ObjectIterator(unreal.SovAccessibilityPresentation) if w.get_world() == world]
                    if surfaces:
                        pc, pawn, _ = self.player(world)
                        where = pawn.get_actor_location()
                        surfaces[0].present_speech(unreal.Text('Lyessa'), unreal.Text(
                            'Hold the line at the relay. If the carriers break through the east stair we lose the whole terrace, and the survivors with it.'),
                            8.0, where, True)
                        surfaces[0].present_caption(unreal.Text('Shield broken'), 8.0, where)
                        self.report['text_presented'] = True
                        self.phase_at = now
                    return
                # One breath so the surface has drawn at least one full frame before the shutter.
                if now-self.phase_at >= SETTLE_SECONDS:
                    # Read at the shutter, so the paint count can be compared against first detection.
                    self.sample(world, 'at_capture')
                    # The UI-inclusive shutter goes first, and alone.
                    #
                    # take_high_res_screenshot routes through FHighResScreenshotConfig, which exposes
                    # resolution, mask and HDR but no UI flag, and the automation path calls
                    # FScreenshotRequest::RequestScreenshot(false) - bShowUI hardcoded off. So no
                    # capture taken that way can ever contain UMG, which is why seven of them showed a
                    # blank HUD that was in fact painting every frame the whole time.
                    #
                    # SHOT/SCREENSHOT does accept it. The hyphen in '-nosuffix' is required:
                    # FParse::Param only matches a token preceded by '-' or '/', so a bare word is
                    # ignored and the engine then generates a numbered filename instead of the one
                    # awaited here. An absolute filename survives CreateViewportScreenShotFilename
                    # because it contains a path separator. In PIE this photographs the whole editor
                    # window, chrome included: it proves the surface renders, but is not a
                    # presentable frame.
                    unreal.SystemLibrary.execute_console_command(
                        world, 'Shot showui -nosuffix filename=' + str(self.ui_shot))
                    self.stage('await_ui_capture')
                return
            if self.phase == 'await_ui_capture':
                # The two shutters are deliberately in different frames. FScreenshotRequest is a
                # global holding one pending filename and one bShowUI, so issuing both together lets
                # the second discard the first - losing the only image that can show the HUD.
                if self.ui_shot.exists() and self.ui_shot.stat().st_size > 0:
                    unreal.AutomationLibrary.take_high_res_screenshot(WIDTH, HEIGHT, str(self.shot))
                    self.stage('await_capture')
                    return
                if now-self.phase_at > SHOT_SECONDS:
                    self.finish(False, 'The UI-inclusive screenshot did not appear within its window')
                    self.stage('end_play')
                return
            if self.phase == 'await_capture':
                # Both shutters must land. Waiting only on the high-res image would declare success
                # while the one image capable of showing UMG was still missing.
                if (self.shot.exists() and self.shot.stat().st_size > 0
                        and self.ui_shot.exists() and self.ui_shot.stat().st_size > 0):
                    self.finish(True, 'Captured the rendered holographic HUD in an ordinary session')
                    self.stage('end_play')
                    return
                if now-self.phase_at > SHOT_SECONDS:
                    self.finish(False, 'The screenshot did not appear within its bounded window')
                    self.stage('end_play')
                return
            if self.phase == 'end_play':
                if editor.is_in_play_in_editor():
                    editor.editor_request_end_play()
                self.stage('await_stopped')
                return
            if self.phase == 'await_stopped':
                if not editor.is_in_play_in_editor() or now-self.phase_at > STOP_SECONDS:
                    self.retire()
                return
        except Exception as error:
            self.report['error'] = str(error)
            self.finish(False, 'Capture failed: ' + str(error))
            # Stop play and release the session rather than leaving the editor running to its timeout.
            self.stage('end_play')

    def start(self):
        # Without this the editor finishes the script and exits before a single tick runs.
        unreal.EditorPythonScripting.set_keep_python_script_alive(True)
        try:
            settings = unreal.GameUserSettings.get_game_user_settings()
            assert isinstance(settings, unreal.SovGameUserSettings)
            # The isolated profile would otherwise hold a first-boot accessibility prerequisite.
            assert settings.complete_accessibility_setup(), 'Cannot accept unchanged standard settings'
            if UI_SCALE:
                snapshot = settings.get_settings_snapshot()
                snapshot.set_editor_property('ui_scale', float(UI_SCALE))
                applied = settings.apply_settings_snapshot(snapshot)
                self.report['ui_scale'] = dict(requested=float(UI_SCALE), applied=str(applied))
            editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            assert not editor.is_in_play_in_editor()
            assert editor.load_level(MAP)
            # A capture without a real viewport would photograph nothing.
            assert editor.get_viewport_config_keys(), 'No real PIE viewport'
            self.write()
            self.handle = unreal.register_slate_post_tick_callback(self.tick)
        except Exception as error:
            self.report['error'] = str(error)
            self.finish(False, 'Startup failed: ' + str(error))
            # No tick is registered to reach the stopped phase, so release the session here.
            self.retire()
        return self


def run(output_directory=None):
    global _RUN
    _RUN = Run(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY']).start()
    return _RUN


if __name__ == '__main__':
    run()
