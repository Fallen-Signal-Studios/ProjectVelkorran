"""Read-only damage share across Aurelion encounters: who damages the hostiles required for victory.

Binds each live required participant's native damage-resolved delegate and attributes applied shield
plus health damage to the current player pawn, a protagonist companion, or another source. It writes no
gameplay state and injects no input. The share is evidence for the TDD's ordinary companion-contribution
band; it does not judge whether that contribution is well tuned.

Object lifetime: a Python delegate binding lasts only as long as its delegate wrapper, so the wrapper is
kept while its participant lives. Keeping wrappers of destroyed actors crashed the editor during garbage
collection after E2's cleanup, so each scan unbinds and drops a participant's wrapper as soon as it is no
longer alive, long before death cleanup destroys it, and forgets every binding when the world changes.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

_RUN = None
SCAN_SECONDS = .25


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
                           method='Native OnDamageResolvedAsTarget receipts only; no input or state writes; wrappers retained only while participants live',
                           encounters={}, receipts=0, rebinds=0, unbound_participants=0)

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

    def observer(self, encounter, participant, target_path):
        def damaged(result):
            try:
                if _path(result.target_actor) != target_path:
                    return
                amount = float(result.applied_health_damage) + float(result.applied_shield_damage)
                if amount <= 0.:
                    return
                world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
                pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
                source = result.source_actor
                if source is not None and pawn is not None and _path(source) == _path(pawn):
                    kind = 'player'
                elif isinstance(source, unreal.SovProtagonistCompanionCharacter):
                    kind = 'companion'
                else:
                    kind = 'other'
                row = self.report['encounters'].setdefault(encounter, dict(player=0., companion=0., other=0., participants={}))
                row[kind] += amount
                row['participants'][participant] = row['participants'].get(participant, 0.) + amount
                self.report['receipts'] += 1
            except Exception:
                self.report.setdefault('callback_errors', []).append(traceback.format_exc()[-400:])
        return damaged

    def scan(self, world):
        live = {}
        for director in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector):
            encounter = str(director.encounter_id)
            for participant in director.participants:
                actor = participant.character
                if not participant.required_for_victory or not unreal.SystemLibrary.is_valid(actor):
                    continue
                live[_path(actor)] = (encounter, str(participant.participant_id), actor)
        # Unbind and drop a wrapper as soon as its participant stops being alive; forget vanished ones.
        for key in list(self.bound):
            row = live.get(key)
            if row is None:
                del self.bound[key]
                self.report['unbound_participants'] += 1
                continue
            actor = row[2]
            if actor.is_actor_being_destroyed() or not actor.is_alive():
                delegate, callback = self.bound.pop(key)
                try:
                    delegate.remove_callable(callback)
                except Exception:
                    pass
                self.report['unbound_participants'] += 1
        for key, (encounter, participant, actor) in live.items():
            if key in self.bound or actor.is_actor_being_destroyed() or not actor.is_alive():
                continue
            asc = actor.get_narrative_ability_system_component()
            if asc is None:
                continue
            callback = self.observer(encounter, participant, key)
            delegate = asc.on_damage_resolved_as_target
            delegate.add_callable(callback)
            # The binding lives only as long as this delegate wrapper.
            self.bound[key] = (delegate, callback)

    def tick(self, _delta):
        if self.done:
            return
        try:
            now = time.monotonic()
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            current = hash(world) if world is not None else None
            if current != self.world_hash:
                # Old-world objects are never touched again; their delegates die with them.
                self.bound = {}
                self.world_hash = current
                self.report['rebinds'] += 1
            if world is not None and now - self.last_scan > SCAN_SECONDS:
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
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if world is not None and hash(world) == self.world_hash:
            try:
                self.scan_release(world)
            except Exception:
                pass
        self.bound = {}
        self.report['status'] = status
        self.write()

    def scan_release(self, world):
        for director in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector):
            for participant in director.participants:
                actor = participant.character
                key = _path(actor) if unreal.SystemLibrary.is_valid(actor) else None
                if key in self.bound:
                    delegate, callback = self.bound[key]
                    delegate.remove_callable(callback)


def start(output_directory):
    global _RUN
    assert _RUN is None or _RUN.done, 'Damage share observer already active'
    _RUN = Run(output_directory)
    _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None:
        _RUN.finish('stopped')
