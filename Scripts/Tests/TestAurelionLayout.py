"""Exercise layout failure boundaries on a host; these tests do not execute Unreal."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('aurelion_layout', ROOT / 'Scripts/Validate-AurelionLayout.py')
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)


class AurelionLayoutTests(unittest.TestCase):
    def setUp(self):
        self.layout = json.loads(VALIDATOR.DEFAULT_MANIFEST.read_text(encoding='utf-8'))

    def rejected(self, message):
        errors = VALIDATOR.validate_layout(self.layout)
        self.assertTrue(any(message in error for error in errors), errors)

    def test_adopted_layout_validates_without_granting_engine_qualification(self):
        original = copy.deepcopy(self.layout)
        self.assertEqual(VALIDATOR.validate_layout(self.layout), [])
        self.assertEqual(self.layout, original)
        self.assertFalse(any(self.layout['qualification'].values()))

    def test_unreachable_departure_and_cycle_rejected(self):
        self.layout['physical_edges'].remove(['Z11', 'Z12'])
        self.rejected('departure')
        self.layout['physical_edges'].append(['Z11', 'Z12'])
        self.layout['physical_edges'].append(['Z08', 'Z05'])
        self.rejected('acyclic')

    def test_separate_perspective_cut_cannot_become_a_physical_door(self):
        self.layout['physical_edges'].append(['Z02', 'Z03'])
        self.rejected('perspective cut')

    def test_streaming_cannot_omit_or_duplicate_zones(self):
        self.layout['streaming']['chunks'][0]['zones'].append('Z05')
        self.rejected('exactly one streaming chunk')

    def test_shared_skeleton_must_be_built_before_entrances(self):
        self.layout['build_passes'][:2] = reversed(self.layout['build_passes'][:2])
        self.rejected('shared zones Z05-Z12 first')

    def test_local_decision_cannot_be_counted_twice(self):
        self.layout['clock']['segments'][4]['minutes'] += 3
        self.rejected('do not add the local decision twice')

    def test_optional_pockets_cannot_gate_progress_or_leave_their_return_zone(self):
        self.layout['clock']['optional_pockets'][2]['required_for_progression'] = True
        self.rejected('without gating progress')
        self.layout['clock']['optional_pockets'][2]['required_for_progression'] = False
        self.layout['clock']['optional_pockets'][2]['return_zone'] = 'Z10'
        self.rejected('rejoin its own zone')

    def test_optional_duration_drift_rejected(self):
        self.layout['clock']['optional_pockets'][3]['minutes'] = 4
        self.rejected('58 minutes')

    def test_reinforcement_cannot_exceed_active_cap(self):
        self.layout['encounters'][0]['waves'][1]['gate']['maximum_alive_before_spawn'] = 3
        self.rejected('exceed the active cap')

    def test_formation_and_rescue_gates_cannot_be_or_conditions(self):
        self.layout['encounters'][2]['waves'][1]['gate']['operator'] = 'any'
        self.rejected('conjunctive')
        self.layout['encounters'][2]['waves'][1]['gate']['operator'] = 'all'
        self.layout['encounters'][2]['waves'][1]['gate']['all_events'] = ['unrelated_event']
        self.rejected('rescue approach open')

    def test_waves_cannot_commit_an_enemy_twice(self):
        self.layout['encounters'][0]['waves'][1]['roster']['ReformationSecurityDrone'] = 1
        self.rejected('commit every hostile exactly once')

    def test_enemy_roles_cannot_silently_drift(self):
        self.layout['encounters'][1]['roster'] = {'DominionEnforcer': 6}
        self.rejected('role composition')

    def test_two_close_relay_disables_are_required(self):
        self.layout['encounters'][1]['relay_receivers'][1]['close_interaction_disable'] = False
        self.rejected('ammunition-independent')

    def test_sensor_failure_cannot_spawn_extra_enemies(self):
        self.layout['encounters'][1]['sensor_failure']['extra_spawns'] = 2
        self.rejected('cannot add enemies')

    def test_elite_cannot_be_respawned_or_handoff_lead_reversed(self):
        self.layout['encounters'][3]['same_elite_preserved_across_handoff'] = False
        self.rejected('preserve the same elite')
        self.layout['encounters'][3]['phases'][1]['lead'] = 'Selene'
        self.rejected('Selene-to-Tarrik')

    def test_checkpoint_alias_or_wrong_zone_rejected(self):
        self.layout['checkpoints'][8]['id'] = 'CP5'
        self.rejected('duplicate identifier')
        self.layout['checkpoints'][8]['id'] = 'CP5b'
        self.layout['checkpoints'][8]['zone'] = 'Z10'
        self.rejected('invalid zone')

    def test_checkpoint_cannot_precede_its_required_handoff(self):
        self.layout['checkpoints'][8]['after_beat'] = 'ThermalFracture'
        self.layout['checkpoints'][8]['before_beat'] = 'HandoffToTarrikCrucible'
        self.rejected('reversed')

    def test_arena_entry_cannot_be_replaced_with_a_second_checkpoint_writer(self):
        self.layout['checkpoints'][1]['native_owner'] = {'class': 'ASovAurelionCheckpoint', 'boundary': 'ExplicitCheckpoint', 'save_key': 'Aurelion.CP1'}
        self.rejected('sole native checkpoint owner')

    def test_choice_cannot_grant_both_benefits_or_kill_unchosen_group(self):
        self.layout['choice']['outcomes'][1]['immediate_support'] = 'west_field_cache'
        self.rejected('never cumulative')
        self.layout['choice']['both_groups_safe'] = False
        self.rejected('both_groups_safe')

    def test_missing_native_name_is_reported_as_source_drift(self):
        source = (ROOT / 'Source/ProjectVelkorran/Private/Campaign/SovAurelionMissionDefinition.cpp').read_text(encoding='utf-8')
        self.assertEqual(VALIDATOR.validate_layout(self.layout, source), [])
        errors = VALIDATOR.validate_layout(self.layout, source.replace('PressureHall', 'RenamedPressureHall'))
        self.assertTrue(any('Native contract token missing: PressureHall' in error for error in errors), errors)

    def test_nan_geometry_and_engine_success_claim_rejected(self):
        self.layout['zones'][0]['geometry']['width_m'] = float('nan')
        self.rejected('finite and positive')
        self.layout['qualification']['unreal_authored'] = True
        self.rejected('cannot claim engine')


if __name__ == '__main__':
    unittest.main()
