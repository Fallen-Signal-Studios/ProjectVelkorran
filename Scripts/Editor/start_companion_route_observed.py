"""Normal-order route with passive companion equipment/movement/damage evidence."""
import json
import os
from pathlib import Path
import runpy
import sys
import time
import traceback
import unreal

_out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'companion-observation.json'
_report = dict(status='waiting', read_only=True, samples=[], damage=[], errors=[])
_state = dict(start=time.monotonic(), last=0., seen=False, bound=set(), handle=None)


def _ref(obj):
    return obj.get_path_name() if obj else None


def _write():
    _out.write_text(json.dumps(_report, indent=2), encoding='utf-8')


def _damage(result):
    _report['damage'].append(dict(elapsed=round(time.monotonic()-_state['start'], 3), result=result.export_text()))


def _stop(reason):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovProtagonistCompanionCharacter):
            if _ref(actor) in _state['bound']:
                actor.get_narrative_ability_system_component().on_damage_resolved_as_source.remove_callable(_damage)
    _state['bound'].clear()
    unreal.unregister_slate_post_tick_callback(_state['handle'])
    _report.update(status='stopped', reason=reason)
    _write()


def _tick(delta):
    now = time.monotonic()
    if now - _state['last'] < .5:
        return
    _state['last'] = now
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            if _state['seen']:
                _stop('PIE ended')
            return
        _state['seen'] = True
        _report['status'] = 'observing'
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovProtagonistCompanionCharacter):
            if actor.get_editor_property('hidden') or not actor.is_alive():
                continue
            asc = actor.get_narrative_ability_system_component()
            if _ref(actor) not in _state['bound']:
                asc.on_damage_resolved_as_source.add_callable(_damage)
                _state['bound'].add(_ref(actor)) # Retain no UObject across a handoff or map travel.
            controller = actor.get_controller()
            row = dict(elapsed=round(now-_state['start'], 3), actor=_ref(actor),
                       position=actor.get_actor_location().export_text(), velocity=actor.get_velocity().export_text(),
                       weapon=_ref(actor.get_weapon()), health=actor.get_health(),
                       inventory=[_ref(item) for item in actor.get_inventory_component().get_items()],
                       curated=[_ref(cls) for cls in actor.get_companion_component().get_editor_property('curated_abilities')],
                       focus=_ref(controller.get_focus_actor()) if controller else None)
            if controller and controller.get_focus_actor():
                row['candidates'] = [candidate.export_text() for candidate in asc.get_bot_attack_candidates(controller.get_focus_actor(), unreal.GameplayTag())]
            _report['samples'].append(row)
        _write()
        if now - _state['start'] > 1800:
            _stop('Observation time bound reached')
    except Exception:
        _report['errors'].append(traceback.format_exc())
        _stop('Observer error')


_state['handle'] = unreal.register_slate_post_tick_callback(_tick)
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Validation' / 'Aurelion'))
import observe_companion_animation
observe_companion_animation.start()
runpy.run_path(str(Path(__file__).with_name('start_aurelion_combat_observed.py')))
