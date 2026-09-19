"""Read actual phase-floor and mesh-overlay transitions during the live route.

No spawning, floor toggles, damage, input, or campaign writes. Start during PIE.
This records presentation state; it does not certify the visual effect or poise.
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
report = dict(status='observing', read_only=True, transitions=[], findings=[])

def write():
    out.write_text(json.dumps(report, indent=2))

def finish(reason):
    report.update(status='observation_finished', reason=reason)
    unreal.unregister_slate_post_tick_callback(handle)
    write()

def tick(delta):
    global last_sample
    now = time.monotonic()
    if now-last_sample < .5:
        return
    last_sample = now
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world or now-started > 1200:
            finish('PIE ended or observer reached its twenty-minute bound')
            return
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase):
            floor = actor.get_component_by_class(unreal.SovLethalFloorComponent)
            if not floor or actor.get_editor_property('hidden') or actor.is_character_pending_load():
                continue
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
