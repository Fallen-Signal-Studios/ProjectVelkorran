"""Read-only legacy readout visibility across every actual route handoff."""
import json
import os
import time
from pathlib import Path
import unreal
import validate_owned_weapon_hud_readonly as owned_hud

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'hud-retirement-route.json'
assert not out.exists()
started = time.monotonic()
last_sample = 0.
saw_world = False
last_state = None
report = dict(status='observing', read_only=True, samples=0, transitions=[], findings=[])


def write():
    out.write_text(json.dumps(report, indent=2))


def finish(reason):
    report.update(status='observation_finished', reason=reason)
    unreal.unregister_slate_post_tick_callback(handle)
    write()


def tick(delta):
    global last_sample, saw_world, last_state
    now = time.monotonic()
    if now-last_sample < 1:
        return
    last_sample = now
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if (not world and saw_world) or now-started > 1200:
            finish('PIE ended or twenty-minute observation bound')
            return
        if not world:
            return
        saw_world = True
        row = owned_hud.sample()
        if 'legacy_readout_retired' not in row:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        state = (pawn.get_path_name() if pawn else None, row['legacy_readout_retired'],
                 row['legacy_container_visibility'], row['legacy_weapon_visibility'])
        report['samples'] += 1
        if state != last_state:
            entry = dict(pawn=state[0], retired=state[1], container=state[2], weapon=state[3], elapsed=now-started)
            report['transitions'].append(entry)
            if not state[1]:
                report['findings'].append(entry)
            last_state = state
        write()
    except Exception as exc:
        report['findings'].append(str(exc))
        finish('Observer error; not acceptance')


write()
handle = unreal.register_slate_post_tick_callback(tick)
