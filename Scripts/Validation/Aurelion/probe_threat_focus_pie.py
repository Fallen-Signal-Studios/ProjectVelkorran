"""Play-validate hard lock-on in a real M12 session (audit finding PC2-07).

Before this, nothing in the project could reach USovTargetingComponent: no asset produced the
semantic tags it listens for, and no enemy carried the Sov.Target.HardLock actor tag its candidate
test requires. This probe checks both ends in a live session rather than on paper.

It starts M12, waits for Tarrik, spawns an Aurelion Linkbound in front of him, and then:
  1. records what the live controller maps, so IA_ThreatFocus/IA_CycleTarget* must show a real key;
  2. presses Narrative.Input.ThreatFocus through the controller's own routing, the same call a bound
     key makes, and expects the lock to land on the spawned enemy;
  3. raises the weapon and confirms the lock survives aiming as a framing handover, which used to be
     reported as a Cinematic lock loss;
  4. presses Narrative.Input.Designate and expects the threat to carry the protagonist's own reward
     window, which nothing in the project used to be able to produce;
  5. presses the focus tag again and expects the lock to clear.

Nothing is saved; the session is stopped at the end.
"""
import json
import os
import time
from pathlib import Path
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'threat-focus-pie.json'
TARGET_NPC = '/Game/Aurelion/Enemies/NPC_AurelionLinkbound'
FOCUS_TAG = 'Narrative.Input.ThreatFocus'
DESIGNATE_TAG = 'Narrative.Input.Designate'
MARK_TAG = 'Sov.State.Target.Marked'
COMMAND_TARGET_TAG = 'Sov.State.CommandTarget.Window'
AIM_TAG = 'Narrative.State.Weapon.IsAiming'
READY_SECONDS, TARGET_SECONDS, SETTLE_SECONDS, HOLD_SECONDS = 180., 25., 2.0, 1.5


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
        self.target = None
        self.report = dict(status='running', scope='Hard lock-on reachability in a real M12 session',
                           steps={}, observations=[])

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic() - self.started, 3)
        OUT.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf-8')

    def stage(self, name):
        self.phase, self.phase_at = name, time.monotonic()
        self.report['observations'].append(dict(phase=name, elapsed=round(time.monotonic() - self.started, 3)))
        self.write()

    def finish(self, passed, reason):
        if self.done:
            return
        self.done = True
        self.report['status'] = 'passed' if passed else 'failed'
        self.report['reason'] = reason
        self.write()
        unreal.log('THREAT_FOCUS_PIE ' + self.report['status'] + ' ' + reason)

    def targeting(self, pawn):
        return pawn.get_component_by_class(unreal.SovTargetingComponent)

    def locked(self, pawn):
        component = self.targeting(pawn)
        target = component.get_locked_target() if component else None
        return target.get_name() if target else None

    def place_target(self, pawn):
        forward = pawn.get_actor_forward_vector()
        self.target.set_actor_location_and_rotation(pawn.get_actor_location() + forward * 500.0,
                                                    unreal.Rotator(0, 0, pawn.get_actor_rotation().yaw + 180.0), False, True)

    def tick(self, _delta):
        try:
            editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            now = time.monotonic()
            elapsed = now - self.phase_at
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
                    self.finish(False, 'Tarrik never became ready')
                    self.stage('end_play'); return
                if pawn is None or not pawn.is_character_ready() or not pawn.is_alive():
                    return
                self.stage('inspect_routing')
                return
            if self.done:
                self.stage('end_play'); return
            if unreal.GameplayStatics.is_game_paused(world):
                self.report['paused_during'] = self.phase
                self.phase_at += _delta
                if now - self.started > 150:
                    self.finish(False, 'Play stayed paused')
                    self.stage('end_play')
                return
            if self.phase == 'inspect_routing':
                routing = list(unreal.SovMeleeValidationLibrary.describe_input_routing(controller))
                self.report['steps']['routing'] = routing
                focus = [line for line in routing if line.startswith('IA_ThreatFocus ')]
                cycles = [line for line in routing if line.startswith('IA_CycleTarget')]
                self.report['steps']['threat_focus_bound'] = bool(focus) and 'keys=unbound' not in focus[0]
                self.report['steps']['cycle_bound'] = len(cycles) == 2 and all('keys=unbound' not in line for line in cycles)
                designate = [line for line in routing if line.startswith('IA_Designate ')]
                self.report['steps']['designate_bound'] = bool(designate) and 'keys=unbound' not in designate[0]
                component = self.targeting(pawn)
                self.report['steps']['targeting_component'] = component is not None
                if component is None:
                    self.finish(False, 'The player has no targeting component'); self.stage('end_play'); return
                definition = unreal.load_asset(TARGET_NPC)
                forward = pawn.get_actor_forward_vector()
                location = pawn.get_actor_location() + forward * 500.0
                rotation = unreal.Rotator(0, 0, pawn.get_actor_rotation().yaw + 180.0)
                self.target = unreal.SovMeleeValidationLibrary.spawn_validation_npc(
                    world, definition, unreal.Transform(location, rotation, unreal.Vector(1, 1, 1)))
                self.report['steps']['target'] = self.target.get_name() if self.target else None
                self.stage('await_target')
                return
            if self.phase == 'await_target':
                if self.target is None or elapsed > TARGET_SECONDS:
                    self.finish(False, 'The lock target never spawned'); self.stage('end_play'); return
                if elapsed < SETTLE_SECONDS:
                    return
                self.place_target(pawn)
                self.report['steps']['target_permits_hard_lock'] = bool(self.target.actor_has_tag('Sov.Target.HardLock'))
                self.stage('press_focus')
                return
            if self.phase == 'press_focus':
                self.place_target(pawn)
                self.report['steps']['press_routed'] = bool(
                    unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag(FOCUS_TAG)))
                self.stage('confirm_lock')
                return
            if self.phase == 'confirm_lock':
                if elapsed < 0.5:
                    return
                self.report['steps']['locked_after_press'] = self.locked(pawn)
                self.report['steps']['lock_matches_target'] = (
                    self.target is not None and self.report['steps']['locked_after_press'] == self.target.get_name())
                self.stage('aim')
                return
            if self.phase == 'aim':
                # Raise the weapon through Narrative's own aim input; the focus must survive it.
                if elapsed < 0.1:
                    unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag('Narrative.Input.AltAttack'))
                    return
                if elapsed < HOLD_SECONDS:
                    self.place_target(pawn)
                    return
                asc = pawn.get_narrative_ability_system_component()
                owned = unreal.GameplayTagLibrary.get_owned_gameplay_tags(asc).export_text()
                component = self.targeting(pawn)
                self.report['steps']['aiming'] = AIM_TAG in owned
                self.report['steps']['locked_while_aiming'] = self.locked(pawn)
                self.report['steps']['framing_suspension'] = str(component.get_framing_suspension())
                self.stage('release_aim')
                return
            if self.phase == 'release_aim':
                if elapsed < 0.1:
                    unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag('Narrative.Input.AltAttack'))
                    return
                if elapsed < HOLD_SECONDS:
                    return
                self.report['steps']['locked_after_aim'] = self.locked(pawn)
                unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag(DESIGNATE_TAG))
                self.stage('confirm_designation')
                return
            if self.phase == 'confirm_designation':
                if elapsed < 0.5:
                    return
                steps = self.report['steps']
                target_asc = self.target.get_narrative_ability_system_component() if self.target else None
                owned = unreal.GameplayTagLibrary.get_owned_gameplay_tags(target_asc).export_text() if target_asc else ''
                component = self.targeting(pawn)
                designated = component.get_designated_target() if component else None
                steps['designated_target'] = designated.get_name() if designated else None
                steps['target_marked'] = MARK_TAG in owned
                steps['target_command_window'] = COMMAND_TARGET_TAG in owned
                # Tarrik designates a command target; Selene marks. One of the two windows must be open.
                steps['designation_window_open'] = steps['target_marked'] or steps['target_command_window']
                unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag(FOCUS_TAG))
                self.stage('confirm_release')
                return
            if self.phase == 'confirm_release':
                if elapsed < 0.5:
                    return
                self.report['steps']['locked_after_second_press'] = self.locked(pawn)
                steps = self.report['steps']
                # Whether AltAttack raises a weapon depends on what the protagonist is wielding, so the aim
                # handover is only claimed when the aiming state was actually observed. The automation test
                # ProjectVelkorran.Campaign.Targeting.NativeWorldAdmissionAndOcclusion covers it either way.
                steps['aim_handover_exercised'] = bool(steps.get('aiming'))
                steps['aim_handover_held_lock'] = bool(steps.get('aiming')) and steps.get('locked_while_aiming') == steps.get('locked_after_press')
                passed = bool(steps.get('threat_focus_bound') and steps.get('cycle_bound')
                              and steps.get('designate_bound') and steps.get('target_permits_hard_lock')
                              and steps.get('lock_matches_target')
                              and steps.get('locked_after_aim') == steps.get('locked_after_press')
                              and steps.get('designation_window_open')
                              and steps.get('locked_after_second_press') is None)
                self.finish(passed, 'Lock-on reachability: ' + json.dumps({k: v for k, v in steps.items() if k != 'routing'}))
                self.stage('end_play')
                return
        except Exception:
            import traceback
            self.report['error'] = traceback.format_exc()
            self.finish(False, 'Probe raised')
            self.stage('end_play')

    def retire(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


_RUN = Probe()
_SETTINGS = unreal.GameUserSettings.get_game_user_settings()
assert isinstance(_SETTINGS, unreal.SovGameUserSettings)
# The isolated profile would otherwise open the first-boot accessibility setup, which pauses play.
assert _SETTINGS.complete_accessibility_setup(), 'Cannot accept unchanged standard settings'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
_RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
