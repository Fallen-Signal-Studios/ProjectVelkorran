"""Observe the HUD through the normal E1 input driver's protagonist handoff.

No possession, journal, health, ammunition or presentation values are injected.
Captures are embedded editor viewports unless their dimensions prove otherwise.
"""
import json
import os
import runpy
import time
import traceback
from pathlib import Path
import unreal

root = Path(__file__).resolve().parent
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert os.environ.get('SOV_AURELION_ENTRY_CONTINUE_E1') == '1'
assert os.environ.get('SOV_AURELION_E1_CONTINUE_ROUTE') != '1'
report = dict(status='running', samples=[], scope=__doc__)
started = time.monotonic()
seen = {}
captured = set()
ended = None


def write():
    (out / 'hud-protagonist-handoff.json').write_text(json.dumps(report, indent=2))


def tick(delta):
    global ended
    now = time.monotonic()
    try:
        if ended is not None:
            if now - ended > 4:
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now - started < 720, 'Normal E1 handoff exceeded the observation deadline'
        route_path = out / 'E1Continuation/e1-input-continuation.json'
        route = json.loads(route_path.read_text()) if route_path.exists() else {}
        assert route.get('status') != 'failed', route.get('reason', 'Normal E1 driver failed')
        entry_path = out / 'entry-result.json'
        entry = json.loads(entry_path.read_text()) if entry_path.exists() else {}
        if entry.get('status') != 'passed':
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():
            return
        name = pawn.get_class().get_name()
        if name not in ('BP_SovTarrik_C', 'BP_SovSelene_C') or name in captured:
            return
        seen.setdefault(name, now)
        if now - seen[name] < 3 or (name == 'BP_SovSelene_C' and route.get('status') != 'passed'):
            return
        surfaces = [w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface)
                    if w.get_world() == world and w.is_in_viewport()]
        assert len(surfaces) == 1, 'Expected exactly one authored HUD after possession'
        surface = surfaces[0]
        view = surface.get_holographic_hud_view()
        protagonist = 'Selene' if 'Selene' in name else 'Tarrik'
        assert protagonist in str(unreal.GameplayTagLibrary.get_tag_name(view.protagonist))
        assert view.valid
        assert abs(view.health.current - pawn.get_health()) < 0.1
        for field, bar in [('HealthBar', view.health), ('ShieldBar', view.shield)]:
            assert abs(surface.get_editor_property(field).get_editor_property('percent') - bar.fraction) < 0.001
        ammo_visible = surface.get_editor_property('AmmoRegion').get_visibility() != unreal.SlateVisibility.COLLAPSED
        assert ammo_visible == view.has_ammo
        if view.has_ammo:
            assert str(surface.get_editor_property('AmmoClip').get_text()) == str(view.ammo_in_clip)
            assert str(surface.get_editor_property('AmmoReserve').get_text()) == str(view.ammo_reserve)
        pixels = unreal.WidgetLayoutLibrary.get_viewport_size(world)
        report['samples'].append(dict(protagonist=protagonist, pawn=name,
            health=view.health.export_text(), shield=view.shield.export_text(),
            palette=view.palette.export_text(), ammo_visible=ammo_visible,
            clip=view.ammo_in_clip, reserve=view.ammo_reserve,
            viewport_pixels=[pixels.x, pixels.y], pips=[p.export_text() for p in view.pips]))
        unreal.SystemLibrary.execute_console_command(world,
            'Shot showui -nosuffix filename=' + str(out / ('hud-' + protagonist.lower() + '.png')))
        captured.add(name)
        if len(captured) == 2:
            report.update(status='passed_requires_visual_review', route_status=route.get('status'))
            ended = now
        write()
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        write()
        ended = now


write()
handle = unreal.register_slate_post_tick_callback(tick)
runpy.run_path(str(root / 'start_companion_mesh_route.py'))
