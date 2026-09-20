"""Controlled first-swing range diagnostic; never qualifies campaign AI.

Uses the existing isolated PIE probe. The target is held at each listed distance,
so these results describe opening animation contact, not ordinary AI movement.
No assets, attack definitions, damage or health values are changed.
"""
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import probe_protagonist_native_melee_pie as probe

probe.TARGET_NPC = '/Game/Aurelion/Enemies/NPC_AurelionLinkbound'
template = probe.WEAPONS[0]
probe.WEAPONS = [dict(template, hero='Tarrik_' + str(distance), clicks=[0.0], distance=distance)
                 for distance in (80, 110, 140, 170)]
probe.CHAIN_SECONDS = 2.0
probe._RUN.report['scope'] = 'Controlled first light swing versus Linkbound at four fixed distances; not campaign or companion-AI qualification'

def place_target(self, pawn):
    self.target.set_actor_location_and_rotation(
        pawn.get_actor_location() + pawn.get_actor_forward_vector() * self.weapon()['distance'],
        unreal.Rotator(0, 0, pawn.get_actor_rotation().yaw + 180.0), False, True)

def summarize(self):
    row = self.row()
    row['distance_cm'] = self.weapon()['distance']
    row['opening_hits'] = [r for r in row['receipts'] if r['phase'] == 'chain']
    row['heavy_hits'] = [r for r in row['receipts'] if r['phase'] == 'heavy']
    # A diagnostic is complete even when no collision occurs. Do not disguise a
    # missing hit as a gameplay pass; the explicit contact result stays separate.
    row['opening_contact_observed'] = bool(row['opening_hits'])
    row['passed'] = bool(row.get('granted_native_only'))
    self.write()
    return row['passed']

probe.Probe.place_target = place_target
probe.Probe.summarize = summarize
