"""Answer one question in a live M12 session: do the authored enemies attack the player?

Starts M12, waits for the protagonist, runs the existing read-only roster audit, then walks the
player's own pawn to each hostile group in turn is NOT attempted - instead the probe samples every
living hostile for the state that decides whether it is fighting: its controller, its current
Narrative activity, its focus, whether it holds the attacking activity tag, and whether the player
loses any health while they are nearby.

This is a diagnostic, not a gate. It reports what it sees and never edits or saves anything.
"""
import json
import os
import time
from pathlib import Path
import sys
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'enemy-aggression-pie.json'
ATTACKING = 'Narrative.State.NPC.Activity.Attacking'
IDLE = 'Narrative.State.NPC.Activity.Idle'
READY_SECONDS, SETTLE_SECONDS, WATCH_SECONDS, SAMPLE_INTERVAL = 180., 45.0, 60.0, 2.0
# Stand the player inside each authored group in turn so proximity cannot be the reason nothing fights.
APPROACH_RANGE = 400.


def _path(obj):
    return obj.get_path_name() if obj else None


class Probe:
    def __init__(self):
        self.started = time.monotonic()
        self.phase, self.phase_at = 'begin', self.started
        self.handle = None
        self.done = False
        self.next_sample = 0.
        self.report = dict(status='running', scope='Do authored M12 enemies attack the player?',
                           roster={}, samples=[], observations=[])

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
        unreal.log('ENEMY_AGGRESSION_PIE ' + reason)

    def hostiles(self, world, player):
        found = []
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NarrativeNPCCharacter):
            try:
                if not actor.is_alive():
                    continue
                if unreal.ArsenalStatics.get_attitude(player, actor) != unreal.TeamAttitude.HOSTILE:
                    continue
            except Exception:
                continue
            found.append(actor)
        return found

    def describe(self, world, player, npc):
        row = dict(name=npc.get_name(), klass=npc.get_class().get_name())
        try:
            row['distance'] = round(player.get_distance_to(npc), 1)
        except Exception:
            row['distance'] = None
        controller = npc.get_controller()
        row['controller'] = controller.get_class().get_name() if controller else None
        try:
            row['focus'] = controller.get_focal_point().__str__() if controller else None
        except Exception:
            row['focus'] = None
        try:
            activities = controller.get_component_by_class(unreal.NPCActivityComponent) if controller else None
            current = activities.get_current_activity() if activities else None
            row['activity'] = current.get_class().get_name() if current else None
            row['activity_count'] = len(activities.get_editor_property('activities')) if activities else 0
        except Exception as exc:
            row['activity'] = 'unreadable: ' + type(exc).__name__
        try:
            asc = npc.get_narrative_ability_system_component()
            owned = unreal.GameplayTagLibrary.get_owned_gameplay_tags(asc).export_text()
            row['attacking'] = ATTACKING in owned
            row['idle'] = IDLE in owned
            row['dead'] = asc.is_dead()
            row['abilities'] = len([h for h in asc.get_all_abilities()])
            row['weapon'] = npc.get_weapon().get_class().get_name() if npc.get_weapon() else None
        except Exception as exc:
            row['abilities'] = 'unreadable: ' + type(exc).__name__
        return row

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
            player = unreal.GameplayStatics.get_player_pawn(world, 0)
            if self.phase == 'await_ready':
                if elapsed > READY_SECONDS:
                    self.finish('The protagonist never became ready')
                    self.stage('end_play'); return
                if player is None or not player.is_character_ready() or not player.is_alive():
                    return
                self.stage('roster')
                return
            if self.done:
                self.stage('end_play'); return
            if self.phase == 'roster':
                # Authored enemies finish their definition, appearance and activity install well after the
                # protagonist is ready. Sampling before that would measure startup, not aggression.
                if elapsed < SETTLE_SECONDS:
                    return
                try:
                    module = __import__('inspect_aurelion_enemies_pie')
                    audit = module.inspect_enemies(world, str(RUN / 'enemy-roster.json'), False)
                    self.report['roster'] = dict(
                        hostiles=len(audit.get('hostiles', [])),
                        startup_pending=audit.get('startup_pending', [])[:20],
                        contract_failures=audit.get('contract_failures', [])[:20],
                        inspection_errors=audit.get('inspection_errors', [])[:10],
                        missing_abilities=[{'actor': row.get('identity') or row.get('actor'),
                                            'missing': row.get('missing_default_abilities')}
                                           for row in audit.get('hostiles', [])
                                           if row.get('missing_default_abilities')][:20])
                except Exception as exc:
                    self.report['roster'] = {'error': type(exc).__name__ + ': ' + str(exc)}
                self.report['player_health_at_start'] = float(player.get_health())
                self.stage('watch')
                return
            if self.phase == 'watch':
                if elapsed >= WATCH_SECONDS:
                    health = float(player.get_health())
                    self.report['player_health_at_end'] = health
                    self.report['player_took_damage'] = health < self.report.get('player_health_at_start', health)
                    attacked = any(row.get('attacking') for sample in self.report['samples'] for row in sample['hostiles'])
                    self.report['any_hostile_entered_attacking'] = attacked
                    self.finish('Attacking observed' if attacked else 'No hostile entered the attacking activity')
                    self.stage('end_play')
                    return
                if elapsed < self.next_sample:
                    return
                self.next_sample = elapsed + SAMPLE_INTERVAL
                hostiles = self.hostiles(world, player)
                # Stand beside the nearest group so distance cannot explain a quiet enemy.
                if hostiles:
                    nearest = min(hostiles, key=lambda npc: player.get_distance_to(npc))
                    if player.get_distance_to(nearest) > APPROACH_RANGE:
                        toward = nearest.get_actor_location() - player.get_actor_location()
                        toward = unreal.Vector(toward.x, toward.y, 0).normal() * APPROACH_RANGE
                        player.set_actor_location(nearest.get_actor_location() - toward, False, False)
                self.report['samples'].append(dict(
                    t=round(elapsed, 2), living_hostiles=len(hostiles),
                    hostiles=[self.describe(world, player, npc) for npc in hostiles[:12]]))
                self.write()
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


sys.path.append(str(Path(__file__).resolve().parent))
_RUN = Probe()
_SETTINGS = unreal.GameUserSettings.get_game_user_settings()
assert isinstance(_SETTINGS, unreal.SovGameUserSettings)
assert _SETTINGS.complete_accessibility_setup(), 'Cannot accept unchanged standard settings'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
_RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
