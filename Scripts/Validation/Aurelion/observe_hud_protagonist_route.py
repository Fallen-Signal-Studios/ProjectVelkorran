"""Observe the real route's HUD across protagonist handoff without driving gameplay.

Start during PIE. Samples actual resource bindings and captures each protagonist once.
Does not modify the view, settings, pawn, mission, or input; leaves the route running.
"""
import json
import os
import time
from pathlib import Path
import unreal

OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
started = time.monotonic()
last_sample = 0.0
last_world = None
seen = {}
report = dict(status='running', scope='Actual route HUD resource bindings and identity; no gameplay writes', protagonists=seen)

def write():
    (OUT/'hud-protagonist-route.json').write_text(json.dumps(report, indent=2, default=str))

def finish(status, reason):
    report.update(status=status, reason=reason)
    unreal.unregister_slate_post_tick_callback(handle)
    write()

def tick(delta):
    global last_sample, last_world
    try:
        now = time.monotonic()
        if now-started > 900:
            finish('incomplete', 'Both protagonists were not observed within 15 minutes')
            return
        if now-last_sample < 1:
            return
        last_sample = now
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            if last_world is not None and now-last_world > 15:
                finish('incomplete', 'PIE ended before both protagonists were observed')
            return
        last_world = now
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn or not pawn.is_character_ready() or pawn.is_character_pending_load():
            return
        surfaces = [w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface)
                    if w.is_in_viewport() and w.get_world() == world]
        if not surfaces:
            return
        assert len(surfaces) == 1
        surface = surfaces[0]
        view = surface.get_holographic_hud_view()
        if not view.valid:
            return
        identity = str(unreal.GameplayTagLibrary.get_tag_name(view.protagonist))
        hero = identity.rsplit('.', 1)[-1]
        assert hero in ('Tarrik', 'Selene'), identity
        bars = {}
        for name, resource in [('HealthBar', view.health), ('ShieldBar', view.shield),
                               ('StaminaBar', view.stamina), ('EchoBar', view.echo)]:
            actual = surface.get_editor_property(name).get_editor_property('percent')
            assert abs(actual-resource.fraction) < .001, (name, actual, resource.fraction)
            bars[name] = dict(displayed=actual, expected=resource.fraction,
                              current=resource.current, maximum=resource.maximum)
        text = str(surface.get_editor_property('ProtagonistName').get_text())
        assert hero.upper() == text.upper().replace(' ', ''), (hero, text)
        if hero not in seen:
            capture = OUT/('hud-route-'+hero.lower()+'.png')
            seen[hero] = dict(identity=identity, name_text=text, bars=bars,
                             pawn=pawn.get_class().get_path_name(), capture=str(capture),
                             health_color=str(view.palette.health_to), shield_color=str(view.palette.shield_to),
                             elapsed=now-started)
            unreal.SystemLibrary.execute_console_command(world, 'Shot showui -nosuffix filename='+str(capture))
            write()
        if len(seen) == 2 and all(Path(row['capture']).exists() for row in seen.values()):
            finish('passed', 'Both actual protagonists matched native resource fractions and identity; images require visual review')
    except Exception as exc:
        finish('failed', str(exc))
        unreal.log_error('HUD_ROUTE_OBSERVER '+str(exc))

write()
handle = unreal.register_slate_post_tick_callback(tick)
