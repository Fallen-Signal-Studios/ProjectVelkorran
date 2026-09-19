"""Compare native camera requests with the actual Blueprint camera interface.

Read-only; never requests/releases claims, changes input or applies camera state.
"""
import json
import os
import time
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'camera-route.json'
assert not out.exists()
report = dict(status='observing', scope=__doc__, transitions=[], findings=[])
started = time.monotonic()
last_sample = 0.
last_state = None
saw_world = False


def write():
    out.write_text(json.dumps(report, indent=2))


def finish(reason):
    report.update(status='observation_finished', reason=reason)
    unreal.unregister_slate_post_tick_callback(handle)
    write()


def tick(delta):
    global last_sample, last_state, saw_world
    now = time.monotonic()
    if now-last_sample < .5:
        return
    last_sample = now
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if (saw_world and not world) or now-started > 1200:
            finish('PIE ended or twenty-minute observation bound')
            return
        if not world:
            return
        saw_world = True
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():
            return
        component = pawn.get_component_by_class(unreal.SovCameraControlComponent)
        assert component
        resolved = component.get_camera_state().export_text()
        supplied = pawn.call_method('Get_CharacterPropertiesForCamera').export_text()
        state = (pawn.get_path_name(), resolved, supplied)
        if state != last_state:
            report['transitions'].append(dict(pawn=state[0], resolved=resolved, supplied=supplied,
                claims=list(component.describe_claims()), revision=component.get_published_revision(),
                elapsed=now-started))
            last_state = state
            write()
    except Exception as exc:
        report['findings'].append(str(exc))
        finish('Observer error; not acceptance')


write()
handle = unreal.register_slate_post_tick_callback(tick)
