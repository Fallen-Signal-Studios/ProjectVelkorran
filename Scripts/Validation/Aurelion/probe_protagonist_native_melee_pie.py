"""Play-validate the protagonists' native melee in a real M12 session (audit finding PC2-01).

Starts M12, waits for Tarrik ready, wields Velkorran and spawns an Aurelion Enforcer in front of him.
The light chain is driven through Narrative's own input entry (UNarrativeAbilitySystemComponent::
AbilityInputTagPressed, the call the player controller makes for IA_Attack), which exercises the combat
input buffer and follow-up windows. SOV_MELEE_INPUT=click sends synthetic Slate clicks instead; in an
unattended, unfocused editor window those reach the viewport but not gameplay input, so the default is
the input entry. Neither validates physical mouse input. The heavy attack is activated on its ability. Then the same is done with Verity, added to
Tarrik's inventory, so Selene's graphs and double-blade trace run on a real character (her montages
share the skeleton). Every damage receipt the player's ASC publishes is recorded with the ability
that produced it. Nothing is saved; the session is stopped at the end.

Pass criteria per weapon: the light ability, not a Narrative combo, lands hits from at least three
distinct chain nodes (distinct attack ids) in one chain; the heavy ability lands a hit classified heavy.
"""
import json
import os
import time
from pathlib import Path
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'protagonist-native-melee-pie.json'
TARGET_NPC = '/Game/Aurelion/Enemies/NPC_AurelionEnforcer'
WEAPONS = [
    dict(hero='Tarrik', item='/Game/Items/Weapons/WI_Velkorran.WI_Velkorran_C',
         light='GA_Tarrik_MeleeLight_C', heavy='/Game/Aurelion/Characters/Melee/GA_Tarrik_MeleeHeavy.GA_Tarrik_MeleeHeavy_C',
         clicks=[0.0, 0.75, 1.5, 2.25]),
    dict(hero='Selene', item='/Game/Items/Weapons/WI_Verity.WI_Verity_C',
         light='GA_Selene_MeleeLight_C', heavy='/Game/Aurelion/Characters/Melee/GA_Selene_MeleeHeavy.GA_Selene_MeleeHeavy_C',
         clicks=[0.0, 0.85, 1.7, 2.55]),
]
READY_SECONDS, WIELD_SECONDS, TARGET_SECONDS = 180., 60., 20.
FOCUS_SETTLE, CHAIN_SECONDS, HEAVY_SECONDS = 2.5, 5.0, 3.0
_RUN = None


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
        self.weapon_index = 0
        self.receipts = []
        self.bound_asc = None
        self.target = None
        self.clicks_sent = 0
        self.report = dict(status='running', scope='Native protagonist melee in a real M12 session',
                           physical_input_validation=False, weapons={}, observations=[])

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic() - self.started, 3)
        OUT.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf-8')

    def stage(self, name):
        self.phase, self.phase_at = name, time.monotonic()
        self.report['observations'].append(dict(phase=name, elapsed=round(time.monotonic() - self.started, 3)))
        self.write()

    def weapon(self):
        return WEAPONS[self.weapon_index]

    def row(self):
        return self.report['weapons'].setdefault(self.weapon()['hero'], dict(receipts=[], notes=[]))

    def finish(self, passed, reason):
        if self.done:
            return
        self.done = True
        self.report['status'] = 'passed' if passed else 'failed'
        self.report['reason'] = reason
        self.write()
        unreal.log('PROTAGONIST_NATIVE_MELEE_PIE ' + self.report['status'] + ' ' + reason)

    # Receipts ----------------------------------------------------------------------------------
    def on_damage(self, result):
        try:
            source_object, source_ability = unreal.SovMeleeValidationLibrary.describe_damage_source(result)
            entry = dict(t=round(time.monotonic() - self.phase_at, 3), phase=self.phase,
                         attack_id=unreal.GuidLibrary.conv_guid_to_string(result.get_editor_property('attack_id')),
                         target=result.get_editor_property('target_actor').get_name() if result.get_editor_property('target_actor') else None,
                         classifications=result.get_editor_property('attack_classifications').export_text(),
                         channels=result.get_editor_property('damage_channels').export_text(),
                         source_object=source_object, source_ability_owner=source_ability)
            for field in ('applied_health_damage', 'applied_shield_damage', 'applied_poise_damage'):
                try:
                    entry[field] = round(float(result.get_editor_property(field)), 2)
                except Exception:
                    pass
            self.row()['receipts'].append(entry)
        except Exception as exc:
            self.row()['notes'].append('receipt read failed: %s' % exc)

    def bind(self, pawn):
        asc = pawn.get_narrative_ability_system_component()
        if asc is not self.bound_asc:
            asc.on_damage_resolved_as_source.add_callable(self.on_damage)
            self.bound_asc = asc
        return asc

    # Helpers -----------------------------------------------------------------------------------
    def player(self, world):
        return unreal.GameplayStatics.get_player_pawn(world, 0)

    def spawn_target(self, world, pawn):
        definition = unreal.load_asset(TARGET_NPC)
        forward = pawn.get_actor_forward_vector()
        location = pawn.get_actor_location() + forward * 150.0
        rotation = unreal.MathLibrary.find_look_at_rotation(location, pawn.get_actor_location())
        rotation = unreal.Rotator(0, 0, rotation.yaw)
        transform = unreal.Transform(location, rotation, unreal.Vector(1, 1, 1))
        return unreal.SovMeleeValidationLibrary.spawn_validation_npc(world, definition, transform)

    def place_target(self, pawn):
        # Square in front at swing distance, facing the player; teleport so collision cannot deflect it.
        forward = pawn.get_actor_forward_vector()
        self.target.set_actor_location_and_rotation(pawn.get_actor_location() + forward * 140.0,
                                                    unreal.Rotator(0, 0, pawn.get_actor_rotation().yaw + 180.0), False, True)

    def alive(self, actor):
        try:
            return actor is not None and actor.is_alive()
        except Exception:
            return actor is not None

    def summarize(self):
        w = self.weapon()
        row = self.row()
        light = [r for r in row['receipts'] if r['phase'] == 'chain']
        heavy = [r for r in row['receipts'] if r['phase'] == 'heavy']
        row['light_attack_ids'] = sorted({r['attack_id'] for r in light})
        row['light_hits'] = len(light)
        row['heavy_hits'] = len(heavy)
        row['heavy_classified'] = any('Sov.Damage.Heavy' in r['classifications'] for r in heavy)
        row['light_sources'] = sorted({str(r['source_ability_owner']) for r in light})
        row['heavy_sources'] = sorted({str(r['source_ability_owner']) for r in heavy})
        row['light_from_native'] = bool(light) and all(r['source_object'] == 'SovEchoAttackReceipt' and r['source_ability_owner'] == w['light'] for r in light)
        row['heavy_from_native'] = bool(heavy) and all(r['source_object'] == 'SovEchoAttackReceipt' and r['source_ability_owner'] == w['heavy'].rsplit('.', 1)[1] for r in heavy)
        row['passed'] = bool(row.get('granted_native_only')) and len(row['light_attack_ids']) >= 3 and row['light_from_native']             and row['heavy_classified'] and row['heavy_from_native']
        self.write()
        return row['passed']

    # Tick --------------------------------------------------------------------------------------
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
            pawn = self.player(world)
            if self.phase == 'await_ready':
                if elapsed > READY_SECONDS:
                    self.finish(False, 'Tarrik never became ready')
                    self.stage('end_play'); return
                if pawn is None or not pawn.is_character_ready() or not pawn.is_alive():
                    return
                self.bind(pawn)
                self.stage('wield')
                return
            if self.done:
                self.stage('end_play'); return
            if unreal.GameplayStatics.is_game_paused(world):
                # Nothing here should pause play; paused time must not count against a phase.
                if not self.report.get('paused_during'):
                    self.report['paused_during'] = self.phase
                    self.write()
                self.phase_at += _delta
                if now - self.started > 150:
                    self.finish(False, 'Play stayed paused')
                    self.stage('end_play')
                return
            w = self.weapon()
            if self.phase == 'wield':
                item_class = unreal.load_class(None, w['item'])
                inventory = pawn.get_component_by_class(unreal.NarrativeInventoryComponent)
                item = inventory.find_item_of_class(item_class, False)
                if item is None:
                    inventory.try_add_item_from_class(item_class, 1, False)
                    item = inventory.find_item_of_class(item_class, False)
                    self.row()['notes'].append('added to inventory for the probe')
                assert item is not None, 'Weapon item unavailable: ' + w['item']
                import re
                slot = str(unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property('current_slot')))
                if slot in ('', 'None'):
                    # Not equipped (Verity joins Tarrik's inventory for this probe): equip through Narrative first.
                    for candidate in re.findall(r'TagName="([^"]+)"', item.get_editor_property('equippable_slots').export_text()):
                        if item.equip_item(tag(candidate)):
                            slot = candidate
                            break
                self.row()['equip_slot'] = slot
                assert slot not in ('', 'None'), 'Weapon could not be equipped'
                if not item.is_wielded():
                    state = unreal.WeaponWieldState()
                    equip = unreal.GameplayTagContainer(); assert equip.import_text('(GameplayTags=((TagName="%s")))' % slot)
                    hands = unreal.GameplayTagContainer(); assert hands.import_text('(GameplayTags=((TagName="Narrative.Equipment.WieldSlot.Mainhand")))')
                    state.set_editor_property('equip_slots', equip)
                    state.set_editor_property('equip_weapons', [item])
                    state.set_editor_property('wield_slots', hands)
                    pawn.set_wield_state(state)
                self.item = item
                self.stage('await_wield')
                return
            if self.phase == 'await_wield':
                if elapsed > WIELD_SECONDS:
                    self.row()['wield_diagnostics'] = dict(
                        wielded=[x.get_class().get_name() for x in pawn.get_wielded_weapons() if x is not None],
                        item_wielded=self.item.is_wielded(), owned_tags=None)
                    self.finish(False, w['hero'] + ' weapon never finished wielding'); return
                asc = self.bind(pawn)
                wielded = [x for x in pawn.get_wielded_weapons() if x is not None]
                if self.item not in wielded:
                    return
                tags = unreal.GameplayTagLibrary.get_owned_gameplay_tags(asc)
                if tags is not None and 'Narrative.State.Weapon.Equipping' in tags.export_text():
                    return
                if elapsed < 1.5:
                    return
                self.row()['wielded'] = [x.get_class().get_name() for x in wielded]
                visual = pawn.get_wielded_weapon_visual(True)
                self.row()['visual'] = dict(name=visual.get_name() if visual else None,
                    mesh=visual.get_editor_property('weapon_mesh').get_skeletal_mesh_asset().get_name() if visual and visual.get_editor_property('weapon_mesh') and visual.get_editor_property('weapon_mesh').get_skeletal_mesh_asset() else None,
                    item_slot=str(unreal.GameplayTagLibrary.get_tag_name(self.item.get_editor_property('current_slot'))))
                if visual is None:
                    return
                granted = {name: list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, tag(name)))
                           for name in ('Narrative.Input.Attack', 'Narrative.Input.Attack.Heavy')}
                self.row()['granted'] = granted
                # The character's own unarmed punch stays granted on the attack input; it is not a weapon combo.
                weapon_melee = [c for c in granted['Narrative.Input.Attack'] if c != 'GA_Melee_Punch_Unarmed_C']
                self.row()['granted_native_only'] = weapon_melee == [w['light']] and granted['Narrative.Input.Attack.Heavy'] == [w['heavy'].rsplit('.', 1)[1]]
                if self.target is None or not self.alive(self.target):
                    self.target = self.spawn_target(world, pawn)
                    self.row()['target'] = self.target.get_name() if self.target else None
                self.stage('await_target')
                return
            if self.phase == 'await_target':
                if elapsed > TARGET_SECONDS or self.target is None:
                    self.finish(False, 'Target never became ready'); return
                if elapsed < 2.0:
                    return
                # Keep the target squarely in front: re-place it at swing distance, then face the player at it.
                forward = pawn.get_actor_forward_vector()
                self.place_target(pawn)
                unreal.SovAurelionPIEInputLibrary.inject_aurelion_pie_left_click(world)  # may only focus the viewport
                self.stage('focus')
                return
            if self.phase == 'focus':
                if elapsed < FOCUS_SETTLE:
                    return
                forward = pawn.get_actor_forward_vector()
                self.place_target(pawn)
                self.row()['receipts'] = [r for r in self.row()['receipts'] if r['phase'] != 'focus']
                self.clicks_sent = 0
                self.stage('chain')
                return
            if self.phase in ('chain', 'heavy'):
                # The live target fights back and its hits can shove the player out of reach, which would
                # measure the enemy rather than the weapon. Hold it at swing distance throughout.
                if self.alive(self.target):
                    self.place_target(pawn)
                asc = pawn.get_narrative_ability_system_component()
                sample = dict(t=round(elapsed, 3), phase=self.phase,
                              melee=list(unreal.SovMeleeValidationLibrary.describe_native_melee(asc)),
                              busy='Narrative.State.Busy' in unreal.GameplayTagLibrary.get_owned_gameplay_tags(asc).export_text())
                try:
                    sample['target_distance'] = round(pawn.get_distance_to(self.target), 1)
                    sample['target_alive'] = self.alive(self.target)
                except Exception:
                    pass
                sample['world_time'] = round(unreal.GameplayStatics.get_time_seconds(world), 3)
                sample['paused'] = unreal.GameplayStatics.is_game_paused(world)
                sample['time_dilation'] = unreal.GameplayStatics.get_global_time_dilation(world)
                trace = self.row().setdefault('trace', [])
                key = [m.split(' interval')[0] if m.startswith('ASC') else m for m in sample['melee']]
                if not trace or trace[-1].get('key') != key or trace[-1]['phase'] != sample['phase'] or int(elapsed * 2) != int(trace[-1]['t'] * 2):
                    sample['key'] = key
                    trace.append(sample)
            if self.phase == 'chain':
                if self.clicks_sent < len(w['clicks']) and elapsed >= w['clicks'][self.clicks_sent]:
                    if os.environ.get('SOV_MELEE_INPUT', 'asc') == 'asc':
                        asc = pawn.get_narrative_ability_system_component()
                        asc.ability_input_tag_pressed(tag('Narrative.Input.Attack')); asc.ability_input_tag_released(tag('Narrative.Input.Attack'))
                        self.row().setdefault('clicks', []).append(dict(t=round(elapsed, 3), routed='asc'))
                    else:
                        result = unreal.SovAurelionPIEInputLibrary.inject_aurelion_pie_left_click(world)
                        self.row().setdefault('clicks', []).append(dict(t=round(elapsed, 3), routed=result.routed))
                    self.clicks_sent += 1
                if elapsed >= CHAIN_SECONDS:
                    self.stage('heavy_prepare')
                return
            if self.phase == 'heavy_prepare':
                if not self.alive(self.target):
                    self.target = self.spawn_target(world, pawn)
                    return
                forward = pawn.get_actor_forward_vector()
                self.place_target(pawn)
                asc = self.bind(pawn)
                heavy = unreal.load_class(None, w['heavy'])
                self.row()['heavy_activated'] = bool(asc.try_activate_ability_by_class(heavy, True))
                self.stage('heavy')
                return
            if self.phase == 'heavy':
                if elapsed < HEAVY_SECONDS:
                    return
                self.summarize()
                if self.weapon_index + 1 < len(WEAPONS):
                    self.weapon_index += 1
                    self.stage('wield')
                    return
                results = {h: r.get('passed') for h, r in self.report['weapons'].items()}
                self.finish(all(results.values()), 'Per-weapon results: ' + json.dumps(results))
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
