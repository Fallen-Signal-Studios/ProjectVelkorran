"""Observe native player damage/exertion during E1; never request gameplay."""
import json
import os
from pathlib import Path
import sys
import time
import traceback
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'combat-observation.json'
report = dict(status='waiting_for_player', gameplay_writes=False, damage=[],
              outgoing=[], stamina_spent=[], samples=[], errors=[])
state = dict(start=time.monotonic(), last=0., written=0., pawn=None,
             bindings=[], handle=None, done=False)

def elapsed():
    return round(time.monotonic()-state['start'], 3)

def write():
    report['elapsed_seconds'] = elapsed()
    out.write_text(json.dumps(report, indent=2), encoding='utf8')

def finish(reason):
    if state['done']: return
    state['done'] = True
    for delegate, callback in state['bindings']:
        delegate.remove_callable(callback)
    state['bindings'].clear()
    state['pawn'] = None
    unreal.unregister_slate_post_tick_callback(state['handle'])
    report.update(status='observation_complete', reason=reason)
    write()

def bind(delegate, callback):
    delegate.add_callable(callback)
    state['bindings'].append((delegate, callback))

def damage(result):
    report['damage'].append(dict(elapsed=elapsed(), result=result.export_text()))

def outgoing(result):
    report['outgoing'].append(dict(elapsed=elapsed(), result=result.export_text()))

def spent(amount, remaining):
    report['stamina_spent'].append(dict(elapsed=elapsed(), amount=amount, remaining=remaining))

def tick(_delta):
    now = time.monotonic()
    if state['done'] or now-state['last'] < .1: return
    state['last'] = now
    try:
        module = sys.modules.get('continue_aurelion_e1_input')
        run = getattr(module, '_RUN', None) if module else None
        if run and run.done:
            finish('E1 driver ended: '+run.report['status'])
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world and state['pawn']:
            finish('Observed PIE world ended')
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
        if pawn and isinstance(pawn, unreal.SovPlayerCharacterBase) and not pawn.is_character_pending_load():
            if state['pawn'] is None:
                state['pawn'] = pawn
                asc = pawn.get_narrative_ability_system_component()
                bind(asc.on_damage_resolved_as_target, damage)
                bind(asc.on_damage_resolved_as_source, outgoing)
                exertion = pawn.get_component_by_class(unreal.SovExertionComponent)
                assert exertion, 'Native exertion component is missing'
                bind(exertion.on_stamina_spent, spent)
                report.update(status='observing', pawn=pawn.get_path_name())
            if pawn != state['pawn']:
                finish('Player pawn changed')
                return
            exertion = pawn.get_component_by_class(unreal.SovExertionComponent)
            velocity = pawn.get_velocity()
            report['samples'].append(dict(elapsed=elapsed(), health=pawn.get_health(),
                stamina=exertion.get_stamina(), position=pawn.get_actor_location().export_text(),
                speed=(velocity.x**2+velocity.y**2+velocity.z**2)**.5, alive=pawn.is_alive()))
            if not pawn.is_alive():
                finish('Observed player death')
                return
        if now-state['start'] > 900.:
            finish('Bounded observation expired; no gameplay qualification implied')
            return
        if now-state['written'] > 1.:
            state['written'] = now
            write()
    except Exception:
        report['errors'].append(traceback.format_exc())
        finish('Observer error; not gameplay evidence')

state['handle'] = unreal.register_slate_post_tick_callback(tick)
