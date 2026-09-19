"""Read actual phase-floor and mesh-overlay transitions during the live route.

No spawning, floor toggles, damage, input, or campaign writes. May start before PIE.
Records actual damage receipts alongside presentation state; visual review is separate.
"""
import json
import os
import time
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'lethal-floor-route.json'
assert not out.exists(), 'Preserve earlier observations'
started = time.monotonic()
last_sample = 0.
states = {}
saw_world = False
damage_bindings = {}
report = dict(status='observing', read_only=True, transitions=[], damage=[], findings=[])

def write():
    out.write_text(json.dumps(report, indent=2))

def finish(reason):
    report.update(status='observation_finished', reason=reason)
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        for asc, callback in damage_bindings.values():
            try:
                if unreal.SystemLibrary.is_valid(asc):
                    asc.on_damage_resolved_as_target.remove_callable(callback)
            except TypeError as exc:
                # PIE teardown can invalidate the Python wrapper before IsValid
                # can marshal it. Its native delegate no longer exists either.
                if 'ObjectInstance is null' not in str(exc):
                    raise
    finally:
        damage_bindings.clear()
        write()

def observe_damage(actor, floor):
    path = actor.get_path_name()
    if path in damage_bindings:
        return
    asc = actor.get_narrative_ability_system_component()
    if not asc:
        return
    def damaged(result):
        if result.target_actor != actor or not unreal.SystemLibrary.is_valid(floor):
            return
        report['damage'].append(dict(actor=path, held=floor.is_floor_held(),
            minimum_health=float(floor.minimum_health), health=actor.get_health(),
            source=result.source_actor.get_path_name() if result.source_actor else None,
            transaction=result.transaction_id.export_text(),
            applied_health=float(result.applied_health_damage), applied_shield=float(result.applied_shield_damage),
            applied_poise=float(result.applied_poise_damage), poise_broken=bool(result.poise_broken),
            fatal=bool(result.fatal), elapsed=time.monotonic()-started))
        write()
    asc.on_damage_resolved_as_target.add_callable(damaged)
    damage_bindings[path] = (asc, damaged)

def tick(delta):
    global last_sample, saw_world
    now = time.monotonic()
    if now-last_sample < .5:
        return
    last_sample = now
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if (not world and saw_world) or now-started > 1200:
            finish('PIE ended or observer reached its twenty-minute bound')
            return
        if not world:
            return
        saw_world = True
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase):
            floor = actor.get_component_by_class(unreal.SovLethalFloorComponent)
            if not floor or actor.get_editor_property('hidden') or actor.is_character_pending_load():
                continue
            observe_damage(actor, floor)
            visual = actor.get_character_visual()
            meshes = list(visual.get_all_meshes()) if visual else []
            overlays = []
            for mesh in meshes:
                material = mesh.get_overlay_material()
                if material:
                    parent = material.get_editor_property('parent') if isinstance(material, unreal.MaterialInstance) else material
                    overlays.append(dict(mesh=mesh.get_path_name(), material=material.get_path_name(), parent=parent.get_path_name() if parent else ''))
            held = floor.is_floor_held()
            key = (held, tuple((r['mesh'], r['parent']) for r in overlays))
            path = actor.get_path_name()
            if states.get(path) != key:
                states[path] = key
                report['transitions'].append(dict(actor=path, held=held, health=actor.get_health(),
                    overlays=overlays, elapsed=now-started))
                write()
    except Exception as exc:
        report['findings'].append(str(exc))
        finish('Observer error; do not treat as acceptance')

write()
handle = unreal.register_slate_post_tick_callback(tick)
