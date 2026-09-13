"""Normal fresh route plus passive weapon and source-attributed damage observation."""
from pathlib import Path
import runpy
import unreal

runpy.run_path(str(Path(__file__).with_name('start_companion_route_observed.py')))
import observe_companion_weapon_hit_path
observe_companion_weapon_hit_path.start()
import observe_elite_core_lifecycle
observe_elite_core_lifecycle.start()

_log_handle = None
def _enable_weapon_log(delta):
    global _log_handle
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        unreal.SystemLibrary.execute_console_command(world, 'log LogSovTransformingWeaponVisual Verbose')
        unreal.unregister_slate_post_tick_callback(_log_handle)
        _log_handle = None

_log_handle = unreal.register_slate_post_tick_callback(_enable_weapon_log)
