"""Read-only damage share across Aurelion encounters: who damages the hostiles required for victory.

Binds each live required participant's native damage-resolved delegate and attributes applied shield
plus health damage to the current player pawn, a protagonist companion, or another source. Bindings are
released whenever the game world changes, so no old world is retained across mission travel or a
checkpoint load. It writes no gameplay state and injects no input. The share is evidence for the TDD's
ordinary companion-contribution band; it does not judge whether that contribution is well tuned.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

_RUN = None


def _path(obj):
    return obj.get_path_name() if obj is not None else None


class Run:
    def __init__(self, output_directory):
        self.out = Path(output_directory)
        self.out.mkdir(parents=True, exist_ok=True)
        self.started = time.monotonic()
        self.done = False
        self.handle = None
        self.world_hash = None
        self.bound = {}
        self.last_scan = 0.
        self.last_write = 0.
        self.report = dict(status='running', scope='Applied shield plus health damage to required encounter hostiles, by source',
                           method='Native OnDamageResolvedAsTarget receipts only; no input or state writes',
                           encounters={}, receipts=0, rebinds=0)

    def write(self):
        for row in self.report['encounters'].values():
            dealt = row['player'] + row['companion']
            row['companion_share_of_protagonist_damage'] = row['companion'] / dealt if dealt > 0. else None
        totals = dict(player=sum(r['player'] for r in self.report['encounters'].values()),
                      companion=sum(r['companion'] for r in self.report['encounters'].values()),
                      other=sum(r['other'] for r in self.report['encounters'].values()))
        dealt = totals['player'] + totals['companion']
        totals['companion_share_of_protagonist_damage'] = totals['companion'] / dealt if dealt > 0. else None
        self.report['totals'] = totals
        self.report['elapsed_seconds'] = round(time.monotonic() - self.started, 3)
        temp = self.out / 'damage-share.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        os.replace(temp, self.out / 'damage-share.json')

    def release(self):
        for delegate, callback in self.bound.values():
            try:
                delegate.remove_callable(callback)
            except Exception:
                pass
        self.bound = {}

    def observer(self, encounter, participant, target_path):
        def damaged(result):
            if _path(result.target_actor) != target_path:
                return
            amount = float(result.applied_health_damage) + float(result.applied_shield_damage)
            if amount <= 0.:
                return
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
            source = result.source_actor
            if source is not None and pawn is not None and source == pawn:
                kind = 'player'
            elif isinstance(source, unreal.SovProtagonistCompanionCharacter):
                kind = 'companion'
            else:
                kind = 'other'
            row = self.report['encounters'].setdefault(encounter, dict(player=0., companion=0., other=0., participants={}))
            row[kind] += amount
            row['participants'][participant] = row['participants'].get(participant, 0.) + amount
            self.report['receipts'] += 1
        return damaged

    def scan(self, world):
        for director in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector):
            encounter = str(director.encounter_id)
            for participant in director.participants:
                actor = participant.character
                if not participant.required_for_victory or not unreal.SystemLibrary.is_valid(actor):
                    continue
                key = _path(actor)
                if key in self.bound:
                    continue
                asc = actor.get_narrative_ability_system_component()
                if asc is None:
                    continue
                callback = self.observer(encounter, str(participant.participant_id), key)
                asc.on_damage_resolved_as_target.add_callable(callback)
                self.bound[key] = (asc.on_damage_resolved_as_target, callback)

    def tick(self, _delta):
        if self.done:
            return
        try:
            now = time.monotonic()
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            current = hash(world) if world is not None else None
            if current != self.world_hash:
                # Never keep old-world delegates or actor wrappers across travel or a reload.
                self.release()
                self.world_hash = current
                self.report['rebinds'] += 1
            if world is not None and now - self.last_scan > 1.:
                self.last_scan = now
                self.scan(world)
            if now - self.last_write > 5.:
                self.last_write = now
                self.write()
        except Exception:
            self.report['error'] = traceback.format_exc()
            self.finish('failed')

    def finish(self, status='stopped'):
        if self.done:
            return
        self.done = True
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.release()
        self.report['status'] = status
        self.write()


def start(output_directory):
    global _RUN
    assert _RUN is None or _RUN.done, 'Damage share observer already active'
    _RUN = Run(output_directory)
    _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None:
        _RUN.finish('stopped')
