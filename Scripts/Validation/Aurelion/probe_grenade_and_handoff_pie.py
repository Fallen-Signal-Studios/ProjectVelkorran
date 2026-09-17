"""Ask a live M12 session why the grenade and the authored handoff do nothing.

Presses Narrative.Input.Ability1 through the controller's own routing, exactly as the G key does,
and records what the ability system did with it: whether the grenade is granted on that input,
whether its spec became active, what the protagonist's Echo balance is against the ability's cost,
and which blocking tags the avatar holds. Then it locates every authored handoff anchor and asks
ASovPlayerController::RequestAuthoredHandoff to refuse, purely to capture the refusal text.

Read-only with respect to content. It activates abilities and requests a handoff, so it is a play
probe, not an audit; nothing is saved and the session is stopped at the end.
"""
import json
import os
import time
from pathlib import Path
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'grenade-and-handoff.json'
ABILITY1 = 'Narrative.Input.Ability1'
READY_SECONDS, SETTLE_SECONDS, OBSERVE_SECONDS = 180., 5.0, 4.0
BLOCKERS = ('Narrative.State.Busy', 'Narrative.State.IsDead', 'Narrative.State.SequencerControlled',
            'Narrative.State.Interacting', 'Sov.State.Fatal', 'Sov.State.Poise.Broken',
            'Sov.State.Evading', 'Sov.State.Guarding', 'Sov.State.Deflecting',
            'Sov.State.Exertion.Exhausted', 'Narrative.State.Weapon.Equipping')


def tag(name):
    result = unreal.GameplayTag()
    assert result.import_text('(TagName="{}")'.format(name))
    return result


class Probe:
    def __init__(self):
        self.started = time.monotonic()
        self.phase, self.phase_at = 'begin', self.started
        self.handle = None
        self.done = False
        self.report = dict(status='running', scope='Why the grenade and authored handoff do nothing',
                           grenade={}, handoff={}, observations=[])

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic() - self.started, 3)
        OUT.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf-8')

    def stage(self, name):
        self.phase, self.phase_at = name, time.monotonic()
        self.report['observations'].append(dict(phase=name, elapsed=round(time.monotonic() - self.started, 3)))
        self.write()

    def finish(self, reason):
        if self.done:
            return
        self.done = True
        self.report['status'] = 'observed'
        self.report['reason'] = reason
        self.write()
        unreal.log('GRENADE_HANDOFF_PIE ' + reason)

    def owned(self, asc):
        try:
            return unreal.GameplayTagLibrary.get_owned_gameplay_tags(asc).export_text()
        except Exception:
            return ''

    def snapshot(self, pawn, label):
        asc = pawn.get_narrative_ability_system_component()
        owned = self.owned(asc)
        row = {'granted_on_ability1': list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, tag(ABILITY1))),
               'blocking_tags_held': [t for t in BLOCKERS if t in owned]}
        for reader, key in ((lambda: float(pawn.get_health()), 'health'),
                            (lambda: float(pawn.get_echo_component().get_echo()), 'echo'),
                            (lambda: float(pawn.get_echo_component().get_maximum_echo()), 'max_echo')):
            try:
                row[key] = reader()
            except Exception as exc:
                row[key] = 'unreadable: ' + type(exc).__name__
        self.report['grenade'][label] = row
        return row

    def tick(self, _delta):
        try:
            editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            elapsed = time.monotonic() - self.phase_at
            if self.phase == 'begin':
                if not editor.is_in_play_in_editor():
                    editor.editor_request_begin_play()
                self.stage('await_ready')
                return
            if self.phase == 'end_play':
                if editor.is_in_play_in_editor():
                    if elapsed < 1.0:
                        editor.editor_request_end_play()
                    return
                self.retire()
                return
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if world is None:
                return
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            if self.phase == 'await_ready':
                if elapsed > READY_SECONDS:
                    self.finish('The protagonist never became ready'); self.stage('end_play'); return
                if pawn is None or not pawn.is_character_ready() or not pawn.is_alive():
                    return
                self.stage('settle')
                return
            if self.done:
                self.stage('end_play'); return
            if self.phase == 'settle':
                if elapsed < SETTLE_SECONDS:
                    return
                self.snapshot(pawn, 'before')
                # The same call a bound key makes, so routing and suppression are exercised too.
                self.report['grenade']['press_routed'] = bool(
                    unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag(ABILITY1)))
                self.stage('observe_press')
                return
            if self.phase == 'observe_press':
                if elapsed < OBSERVE_SECONDS:
                    return
                self.snapshot(pawn, 'after_press')
                asc = pawn.get_narrative_ability_system_component()
                # Ask the ability system directly, which reports whether activation itself is possible.
                granted = list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, tag(ABILITY1)))
                self.report['grenade']['direct_activation'] = {}
                for entry in granted:
                    for path in ('/Game/Abilities/Tarrik/GA_Tarrik_CinderStickyGrenade.GA_Tarrik_CinderStickyGrenade_C',
                                 '/Game/Abilities/Selene/GA_Selene_StillpointGrenade.GA_Selene_StillpointGrenade_C'):
                        cls = unreal.load_class(None, path)
                        if cls and cls.get_name() == entry:
                            self.report['grenade']['direct_activation'][entry] = bool(asc.try_activate_ability_by_class(cls, True))
                self.stage('observe_activation')
                return
            if self.phase == 'observe_activation':
                if elapsed < OBSERVE_SECONDS:
                    return
                self.snapshot(pawn, 'after_direct_activation')
                anchors = []
                for anchor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovCampaignHandoffAnchor):
                    row = {'anchor': anchor.get_name(), 'distance': round(pawn.get_distance_to(anchor), 1)}
                    try:
                        accepted, error = controller.request_authored_handoff(anchor)
                        row['accepted'] = bool(accepted)
                        row['refusal'] = str(error)
                    except Exception as exc:
                        row['refusal'] = 'unavailable: ' + type(exc).__name__ + ': ' + str(exc)
                    anchors.append(row)
                self.report['handoff']['anchors'] = sorted(anchors, key=lambda r: r['distance'])[:10]
                self.finish('Grenade and handoff state recorded')
                self.stage('end_play')
                return
        except Exception:
            import traceback
            self.report['error'] = traceback.format_exc()
            self.finish('Probe raised')
            self.stage('end_play')

    def retire(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


_RUN = Probe()
_SETTINGS = unreal.GameUserSettings.get_game_user_settings()
assert isinstance(_SETTINGS, unreal.SovGameUserSettings)
assert _SETTINGS.complete_accessibility_setup(), 'Cannot accept unchanged standard settings'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
_RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
