"""Focused M12/M13 PIE smoke test of each playable Echo ability in weapon context.

Uses the real ready hero, inventory, wield state, controller semantic input, and
GAS; only the Echo refill and stationary validation target are synthetic fixtures.
It records montage, projectile actor, Niagara and target-health observations.
"""
import json
import os
import hashlib
import shutil
import sys
import time
import traceback
from pathlib import Path

import unreal


RUN = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
OUT = RUN / 'player-abilities-pie.json'
HERO = os.environ.get('SOV_PLAYER_ABILITY_HERO', 'Tarrik')
E2_SOURCE = Path(os.environ.get('SOV_AURELION_E2_SOURCE', str(
    Path(unreal.Paths.project_dir()) / 'Saved/Validation/Aurelion/E2EnforcerFacingReplay-20260923-030228-c85d6e38')))
TARGET = '/Game/Aurelion/Enemies/NPC_AurelionEnforcer'
ROSTER = {
    'Tarrik': [
        ('CinderStickyGrenade', 'Velkorran', 'Ability1', 900, 35, '/Game/Abilities/Presentation/BP_CinderGrenadeProjectile.BP_CinderGrenadeProjectile_C'),
        ('CinderStickyGrenade', 'Cinderline', 'Ability1', 900, 35, '/Game/Abilities/Presentation/BP_CinderGrenadeProjectile.BP_CinderGrenadeProjectile_C'),
        ('VelkorransHunger', 'Velkorran', 'Ability2', 700, 50, '/Game/Abilities/Presentation/BP_VelkorransHungerProjectile.BP_VelkorransHungerProjectile_C'),
        ('CinderSlam', 'Velkorran', 'Ability3', 250, 90, None),
        ('CinderJudgement', 'Cinderline', 'Ability2', 900, 50, None),
        ('CinderlineRequiem', 'Cinderline', 'Ability3', 800, 90, '/Game/Abilities/Presentation/BP_CinderlineRequiemLine.BP_CinderlineRequiemLine_C'),
    ],
    'Selene': [
        ('StillpointGrenade', 'Verity', 'Ability1', 1150, 35, '/Game/Abilities/Presentation/BP_StillpointProjectile.BP_StillpointProjectile_C'),
        ('StillpointGrenade', 'Staccato', 'Ability1', 1150, 35, '/Game/Abilities/Presentation/BP_StillpointProjectile.BP_StillpointProjectile_C'),
        ('StillpointGrenade', 'Axiom', 'Ability1', 1150, 35, '/Game/Abilities/Presentation/BP_StillpointProjectile.BP_StillpointProjectile_C'),
        ('VeritysWake', 'Verity', 'Ability2', 300, 30, '/Game/Abilities/Presentation/BP_VeritysWakeProjectile.BP_VeritysWakeProjectile_C'),
        ('StaccatoZero', 'Staccato', 'Ability2', 900, 30, None),
        ('AxiomNullPulse', 'Axiom', 'Ability2', 900, 30, None),
        ('Dispatch', 'Verity', 'Ability3', 800, 90, '/Game/Abilities/Presentation/BP_DispatchProjectile.BP_DispatchProjectile_C'),
    ],
}
assert HERO in ROSTER
ONLY = os.environ.get('SOV_PLAYER_ABILITY_ONLY')
if ONLY:
    ROSTER[HERO] = [row for row in ROSTER[HERO] if row[0] == ONLY]
    assert ROSTER[HERO], 'No requested ability in roster: ' + ONLY
WEAPON_ONLY = os.environ.get('SOV_PLAYER_ABILITY_WEAPON')
if WEAPON_ONLY:
    ROSTER[HERO] = [row for row in ROSTER[HERO] if row[1] == WEAPON_ONLY]
    assert ROSTER[HERO], 'No requested weapon in roster: ' + WEAPON_ONLY
DISTANCE_OVERRIDE = os.environ.get('SOV_PLAYER_ABILITY_DISTANCE')
if DISTANCE_OVERRIDE:
    distance = float(DISTANCE_OVERRIDE)
    assert 100.0 <= distance <= 2000.0
    ROSTER[HERO] = [(name, weapon, action, distance, cost, projectile)
                    for name, weapon, action, _, cost, projectile in ROSTER[HERO]]
CAPTURE = os.environ.get('SOV_PLAYER_ABILITY_CAPTURE') == '1'
NEGATIVE_ECHO = os.environ.get('SOV_PLAYER_ABILITY_NEGATIVE_ECHO') == '1'


def tag(name):
    result = unreal.GameplayTag()
    assert result.import_text('(TagName="Narrative.Input.' + name + '")')
    return result


class Probe:
    def __init__(self):
        self.started = time.monotonic()
        self.phase = 'begin'
        self.phase_at = self.started
        self.index = 0
        self.repetition = 0
        self.handle = None
        self.item = None
        self.target = None
        self.last_sample = 0
        self.e2_load_started = False
        self.retry = None
        self.bound_asc = None
        self.preexisting_niagara = set()
        self.report = {'status': 'running', 'hero': HERO, 'qualification': 'PIE fixture, synthetic Echo refill and target',
                       'abilities': [], 'observations': []}

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic() - self.started, 2)
        OUT.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf-8')

    def stage(self, phase):
        self.phase = phase
        self.phase_at = time.monotonic()
        self.report['observations'].append({'phase': phase, 'index': self.index,
                                            'elapsed': round(self.phase_at - self.started, 2)})
        self.write()

    def row(self):
        return self.report['abilities'][-1]

    def finish(self, status, reason):
        if self.bound_asc:
            self.bound_asc.on_damage_resolved_as_source.remove_callable(self.on_damage)
            self.bound_asc = None
        if self.retry:
            self.retry.stop()
            self.retry = None
        self.report['status'] = status
        self.report['reason'] = reason
        self.write()
        self.stage('end_play')

    def prep_weapon(self, pawn, weapon):
        item_class = unreal.load_class(None, '/Game/Items/Weapons/WI_' + weapon + '.WI_' + weapon + '_C')
        inventory = pawn.get_component_by_class(unreal.NarrativeInventoryComponent)
        item = inventory.find_item_of_class(item_class, False)
        if item is None:
            raise RuntimeError('Missing signature weapon ' + weapon)
        slot = str(unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property('current_slot')))
        if slot in ('', 'None'):
            raise RuntimeError('Signature weapon has no equipped slot: ' + weapon)
        if not item.is_wielded():
            state = unreal.WeaponWieldState()
            slots = unreal.GameplayTagContainer()
            assert slots.import_text('(GameplayTags=((TagName="%s")))' % slot)
            hands = unreal.GameplayTagContainer()
            assert hands.import_text('(GameplayTags=((TagName="Narrative.Equipment.WieldSlot.Mainhand")))')
            state.set_editor_property('equip_slots', slots)
            state.set_editor_property('equip_weapons', [item])
            state.set_editor_property('wield_slots', hands)
            pawn.set_wield_state(state)
        self.item = item
        self.row()['weapon_slot'] = slot

    def on_damage(self, result):
        try:
            source_object, source_ability = unreal.SovMeleeValidationLibrary.describe_damage_source(result)
            target = result.get_editor_property('target_actor')
            entry = {'target': target.get_name() if target else None,
                     'source_object': source_object, 'source_ability': source_ability}
            for field in ('applied_health_damage', 'applied_shield_damage', 'applied_poise_damage'):
                entry[field] = round(float(result.get_editor_property(field)), 2)
            self.row().setdefault('damage_receipts', []).append(entry)
        except Exception as exc:
            self.row().setdefault('damage_errors', []).append(str(exc))

    def spawn_target(self, world, pawn, distance):
        definition = unreal.load_asset(TARGET)
        pos = pawn.get_actor_location() + pawn.get_actor_forward_vector() * distance
        facing = unreal.MathLibrary.find_look_at_rotation(pos, pawn.get_actor_location())
        transform = unreal.Transform(pos, unreal.Rotator(0, 0, facing.yaw), unreal.Vector(1, 1, 1))
        actor = unreal.SovMeleeValidationLibrary.spawn_validation_npc(world, definition, transform)
        if actor:
            try:
                actor.get_character_movement().disable_movement()
            except Exception:
                pass
        return actor

    def load_earned_e2(self, world):
        prior = json.loads((E2_SOURCE / 'E1Continuation/route-follow-on/continue_aurelion_e2_input/'
                            'e2-input-continuation.json').read_text(encoding='utf-8'))
        if prior['status'] != 'passed':
            raise RuntimeError('E2 checkpoint source was not earned')
        banks = sorted((E2_SOURCE / 'UserData/Saved/SaveGames').glob('*_2_0_*.sav'))
        if len(banks) != 2:
            raise RuntimeError('Expected exactly two earned E2 checkpoint banks')
        destination = (RUN / 'UserData/Saved/SaveGames').resolve()
        if not destination.is_relative_to(RUN.resolve()):
            raise RuntimeError('Save destination escaped isolated profile')
        destination.mkdir(parents=True, exist_ok=True)
        hashes = []
        for bank in banks:
            target = destination / bank.name
            shutil.copy2(bank, target)
            digest = hashlib.sha256(bank.read_bytes()).hexdigest()
            if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
                raise RuntimeError('Copied checkpoint hash differs')
            hashes.append({'name': bank.name, 'sha256': digest})
        instance = unreal.GameplayStatics.get_game_instance(world)
        saves = next(obj for obj in unreal.ObjectIterator(unreal.SovSaveSubsystem) if obj.get_outer() == instance)
        result, message = saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
        if result != unreal.SovSaveResult.LOAD_STARTED:
            raise RuntimeError('Public E2 load refused: ' + str(result) + ' ' + str(message))
        self.e2_load_started = True
        self.report['earned_e2'] = {'source': str(E2_SOURCE), 'banks': hashes,
                                    'load_result': str(result), 'initial_world': world.get_name()}
        self.write()

    def sample(self, world, pawn):
        row = self.row()
        try:
            anim = pawn.get_editor_property('mesh').get_anim_instance()
            montage = anim.get_current_active_montage() if anim else None
            if montage:
                row.setdefault('montages_seen', [])
                if montage.get_name() not in row['montages_seen']:
                    row['montages_seen'].append(montage.get_name())
        except Exception as exc:
            errors = row.setdefault('sample_errors', [])
            if len(errors) < 3:
                errors.append('montage: ' + str(exc))
        cls_path = ROSTER[HERO][self.index][5]
        if cls_path:
            actor_type = unreal.load_class(None, cls_path)
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, actor_type):
                if actor.get_name() not in row.setdefault('projectiles_seen', []):
                    row['projectiles_seen'].append(actor.get_name())
                    row.setdefault('projectile_samples', []).append({
                        'actor': actor.get_name(), 'location': actor.get_actor_location().export_text(),
                        'phase': str(actor.get_payload_phase()) if isinstance(actor, unreal.SovSeleneCombatProjectile) else None})
        for component in unreal.ObjectIterator(unreal.NiagaraComponent):
            if component.get_world() != world:
                continue
            if component.get_path_name() in self.preexisting_niagara:
                continue
            system = component.get_editor_property('asset')
            if not system:
                continue
            name = system.get_path_name()
            if not any(part in name for part in ('/Fire_Magic/', '/Ice_Magic/', '/Lightning_Magic/')):
                continue
            if name not in row.setdefault('niagara_seen', []):
                row['niagara_seen'].append(name)
        if self.target:
            try:
                row['target_health_last'] = round(float(self.target.get_health()), 2)
                shield = self.target.get_component_by_class(unreal.SovShieldComponent)
                row['target_shield_last'] = round(float(shield.get_shield()), 2) if shield else None
            except Exception:
                pass

    def tick(self, delta):
        try:
            editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            elapsed = time.monotonic() - self.phase_at
            if self.phase == 'begin':
                editor.editor_request_begin_play()
                self.stage('await_ready')
                return
            if self.phase == 'end_play':
                if editor.is_in_play_in_editor():
                    editor.editor_request_end_play()
                    return
                unreal.unregister_slate_post_tick_callback(self.handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
                return
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if world is None:
                return
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            if self.phase == 'await_ready':
                if elapsed > 180:
                    self.finish('failed', 'Player never became ready')
                    return
                if not pawn or not pawn.is_character_ready() or pawn.is_character_pending_load():
                    return
                tags = unreal.GameplayTagLibrary.get_owned_gameplay_tags(pawn.get_narrative_ability_system_component()).export_text()
                if 'Sov.Character.Player.' + HERO not in tags:
                    if HERO == 'Selene' and not self.e2_load_started:
                        self.load_earned_e2(world)
                        return
                    if self.e2_load_started:
                        return
                    self.finish('wrong_hero', 'Map started with another protagonist: ' + tags)
                    return
                if HERO == 'Selene' and self.e2_load_started:
                    directors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector)
                                 if str(a.encounter_id) == 'M12_E2_RelayOverlook']
                    if len(directors) != 1:
                        return
                    state = directors[0].get_encounter_state()
                    if state == unreal.SovEncounterState.FAILED:
                        if not self.retry:
                            sys.path.insert(0, str(Path(unreal.Paths.project_dir()) / 'Scripts/Validation/Aurelion'))
                            from aurelion_retry_input import RetryInput
                            self.retry = RetryInput(world, directors[0], RUN / 'E2RetryInput')
                        controller = unreal.GameplayStatics.get_player_controller(world, 0)
                        if not self.retry.step(world, controller, pawn):
                            return
                        self.retry.stop()
                        self.retry = None
                    if directors[0].get_encounter_state() != unreal.SovEncounterState.ACTIVE:
                        return
                if not pawn.is_alive():
                    return
                self.stage('prepare')
                return
            if not pawn or not pawn.is_alive():
                self.finish('failed', 'Player died during ability probe')
                return
            name, weapon, action, distance, cost, _ = ROSTER[HERO][self.index]
            asc = pawn.get_narrative_ability_system_component()
            if self.phase == 'prepare':
                self.report['abilities'].append({'name': name, 'weapon': weapon, 'action': action,
                                                 'repetition': self.repetition + 1,
                                                 'expected_cost': cost, 'projectiles_seen': [],
                                                 'montages_seen': [], 'niagara_seen': []})
                self.prep_weapon(pawn, weapon)
                self.stage('await_wield')
                return
            if self.phase == 'await_wield':
                if elapsed > 20:
                    raise RuntimeError('Wield timeout: ' + weapon)
                if not self.item.is_wielded() or elapsed < 1.2:
                    return
                owned = unreal.GameplayTagLibrary.get_owned_gameplay_tags(asc).export_text()
                if 'Narrative.State.Weapon.Equipping' in owned:
                    return
                granted = list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, tag(action)))
                if self.bound_asc is None:
                    asc.on_damage_resolved_as_source.add_callable(self.on_damage)
                    self.bound_asc = asc
                self.row()['granted_on_input'] = granted
                wanted = 'GA_' + HERO + '_' + name + '_C'
                self.row()['expected_grant_present'] = wanted in granted
                if not self.row()['expected_grant_present']:
                    raise RuntimeError('Expected ability not granted for ' + weapon + ': ' + str(granted))
                pawn.get_echo_component().restore_echo_from_checkpoint(100.)
                self.target = self.spawn_target(world, pawn, distance)
                self.row()['target_name'] = self.target.get_name() if self.target else None
                self.row()['target_health_before'] = float(self.target.get_health()) if self.target else None
                try:
                    shield = self.target.get_component_by_class(unreal.SovShieldComponent) if self.target else None
                    self.row()['target_shield_before'] = float(shield.get_shield()) if shield else None
                except Exception:
                    pass
                self.row()['echo_before'] = round(pawn.get_echo_component().get_echo(), 2)
                self.preexisting_niagara = {component.get_path_name()
                                            for component in unreal.ObjectIterator(unreal.NiagaraComponent)
                                            if component.get_world() == world}
                ability_asset = unreal.load_asset('/Game/Abilities/' + HERO + '/GA_' + HERO + '_' + name)
                self.row()['expected_cast_fx'] = unreal.get_default_object(ability_asset.generated_class()).get_editor_property('cast_niagara_system').get_path_name()
                controller = unreal.GameplayStatics.get_player_controller(world, 0)
                if self.target:
                    eyes, _ = pawn.get_actor_eyes_view_point()
                    aim_point = self.target.get_actor_location() + unreal.Vector(0., 0., 35.)
                    control = unreal.MathLibrary.find_look_at_rotation(
                        eyes, aim_point)
                    controller.set_control_rotation(control)
                    self.row()['aim_rotation'] = control.export_text()
                    self.row()['player_location'] = pawn.get_actor_location().export_text()
                    self.row()['target_location'] = self.target.get_actor_location().export_text()
                if NEGATIVE_ECHO:
                    pawn.get_echo_component().restore_echo_from_checkpoint(0.)
                    self.row()['insufficient_echo'] = {
                        'before': round(pawn.get_echo_component().get_echo(), 2),
                        'input_routed': bool(unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag(action)))}
                    self.stage('observe_insufficient')
                else:
                    self.row()['routed'] = bool(unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag(action)))
                    self.sample(world, pawn)
                    self.stage('observe')
                return
            if self.phase == 'observe_insufficient':
                if time.monotonic() - self.last_sample >= 0.01:
                    self.sample(world, pawn)
                    self.last_sample = time.monotonic()
                if elapsed < 1.5:
                    return
                row = self.row()
                negative = row['insufficient_echo']
                negative['after'] = round(pawn.get_echo_component().get_echo(), 2)
                negative['projectiles'] = list(row['projectiles_seen'])
                negative['cast_montages'] = [name for name in row['montages_seen']
                                             if name.startswith('AM_' + HERO + '_' + row['name'] + '_')]
                negative['damage_receipts'] = [receipt for receipt in row.get('damage_receipts', [])
                                               if receipt['target'] == row['target_name']]
                negative['other_damage_receipts'] = [receipt for receipt in row.get('damage_receipts', [])
                                                     if receipt['target'] != row['target_name']]
                negative['passed'] = negative['input_routed'] and negative['before'] == 0. and negative['after'] == 0. \
                    and not negative['projectiles'] and not negative['cast_montages'] and not negative['damage_receipts']
                if not negative['passed']:
                    self.finish('failed', 'Insufficient-Echo activation produced an effect or did not route: ' + str(negative))
                    return
                row['projectiles_seen'].clear()
                row['projectile_samples'] = []
                row['montages_seen'].clear()
                row['niagara_seen'].clear()
                row.pop('damage_receipts', None)
                self.preexisting_niagara = {component.get_path_name()
                                            for component in unreal.ObjectIterator(unreal.NiagaraComponent)
                                            if component.get_world() == world}
                pawn.get_echo_component().restore_echo_from_checkpoint(100.)
                row['echo_before'] = round(pawn.get_echo_component().get_echo(), 2)
                controller = unreal.GameplayStatics.get_player_controller(world, 0)
                row['routed'] = bool(unreal.SovMeleeValidationLibrary.press_and_release_semantic_input(controller, tag(action)))
                self.sample(world, pawn)
                self.stage('observe')
                return
            if self.phase == 'observe':
                if CAPTURE and self.repetition == 0 and not self.row().get('capture_attempted') and elapsed >= .15:
                    self.row()['capture_attempted'] = True
                    png = RUN / (HERO + '-' + name + '-' + weapon + '-cast.png')
                    try:
                        result = unreal.SovAurelionPIEInputLibrary.capture_aurelion_pie_viewport_with_ui(world, str(png))
                        self.row()['capture'] = {'path': str(png), 'captured': bool(result.captured)}
                    except Exception as exc:
                        self.row()['capture'] = {'error': str(exc)}
                    self.write()
                if time.monotonic() - self.last_sample >= 0.01:
                    self.sample(world, pawn)
                    self.last_sample = time.monotonic()
                if elapsed < 3.2:
                    return
                row = self.row()
                row['echo_after'] = round(pawn.get_echo_component().get_echo(), 2)
                row['echo_spent'] = round(row['echo_before'] - row['echo_after'], 2)
                row['expected_cast'] = 'AM_' + HERO + '_' + name + '_' + ('A' if self.repetition == 0 else 'B')
                row['cast_seen'] = row['expected_cast'] in row['montages_seen']
                row['cast_fx_seen'] = row['expected_cast_fx'] in row['niagara_seen']
                row['projectile_seen'] = bool(row['projectiles_seen']) if ROSTER[HERO][self.index][5] else None
                row['passed_smoke'] = row['routed'] and row['expected_grant_present'] and abs(row['echo_spent'] - cost) < .1 \
                    and row['cast_seen'] and row['cast_fx_seen'] and (row['projectile_seen'] is not False)
                if NEGATIVE_ECHO:
                    row['passed_smoke'] = row['passed_smoke'] and row['insufficient_echo']['passed']
                if self.target:
                    try:
                        self.target.destroy_actor()
                    except Exception:
                        pass
                    self.target = None
                self.write()
                self.repetition += 1
                if self.repetition >= 2:
                    self.repetition = 0
                    self.index += 1
                if self.index >= len(ROSTER[HERO]):
                    self.finish('passed' if all(r.get('passed_smoke') for r in self.report['abilities']) else 'partial',
                                f'{len(ROSTER[HERO])} weapon contexts exercised twice; inspect per-ability evidence')
                else:
                    self.stage('prepare')
                return
        except Exception:
            self.report['error'] = traceback.format_exc()
            self.finish('failed', 'Probe raised')


_SETTINGS = unreal.GameUserSettings.get_game_user_settings()
assert isinstance(_SETTINGS, unreal.SovGameUserSettings)
assert _SETTINGS.complete_accessibility_setup()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
_RUN = Probe()
_RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
