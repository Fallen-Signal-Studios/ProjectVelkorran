"""Read-only native outgoing damage observation for the live Eclipse roster."""
import json
import os
from pathlib import Path
import time
import sys
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'eclipse-damage.json'
assert not out.exists(), 'Preserve the existing damage observation'
report = dict(status='observing', gameplay_writes=False, bindings=[], damage=[], errors=[])
state = dict(start=time.monotonic(), last=0., handle=None, bindings={}, world=None)
roles = {'BP_Aurelion'+r+'_C':r for r in ('Linkbound','WallRunner','Weaver','Elite')}


def callback_for(path, role):
    def callback(result):
        report['damage'].append(dict(elapsed=time.monotonic()-state['start'],
            actor=path, role=role, result=result.export_text()))
    return callback


def finish(reason):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase) if world else []
    for actor in actors:
        callback = state['bindings'].get(actor.get_path_name())
        if callback is None:
            continue
        try:
            asc = actor.get_narrative_ability_system_component()
            if asc:
                asc.on_damage_resolved_as_source.remove_callable(callback)
        except Exception as error:
            report['errors'].append(str(error))
    state['bindings'].clear()
    unreal.unregister_slate_post_tick_callback(state['handle'])
    report.update(status='observation_complete', reason=reason)
    out.write_text(json.dumps(report, indent=2), encoding='utf8')


def tick(_delta):
    now = time.monotonic()
    if now-state['last'] < 1.: return
    state['last'] = now
    final_module = sys.modules.get('continue_aurelion_e4b_input')
    final_run = getattr(final_module, '_RUN', None) if final_module else None
    if final_run and final_run.done:
        finish('E4B driver ended: '+final_run.report['status'])
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    path = world.get_path_name() if world else None
    if state['world'] is not None and path != state['world']:
        finish('Observed PIE world ended or changed')
        return
    if now-state['start'] > 3600.:
        finish('Bounded observation ended')
        return
    if world:
        state['world'] = path
        # Even IsValid cannot accept a Python wrapper whose native object has
        # already been collected. Retain only paths and Python callbacks across
        # ticks; reacquire actors and delegates from the current world each time.
        actors = {actor.get_path_name(): actor for actor in
            unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase)}
        for key, callback in list(state['bindings'].items()):
            actor = actors.get(key)
            if actor is None or not actor.is_alive():
                asc = actor.get_narrative_ability_system_component() if actor else None
                if asc:
                    asc.on_damage_resolved_as_source.remove_callable(callback)
                del state['bindings'][key]
        for key, actor in actors.items():
            role = roles.get(actor.get_class().get_name())
            if not role or key in state['bindings'] or actor.is_character_pending_load() or not actor.is_alive(): continue
            asc = actor.get_narrative_ability_system_component()
            if not asc: continue
            callback = callback_for(key, role)
            asc.on_damage_resolved_as_source.add_callable(callback)
            state['bindings'][key] = callback
            report['bindings'].append(dict(actor=key, role=role, elapsed=now-state['start']))
    report['elapsed_seconds'] = now-state['start']
    out.write_text(json.dumps(report, indent=2), encoding='utf8')


state['handle'] = unreal.register_slate_post_tick_callback(tick)
