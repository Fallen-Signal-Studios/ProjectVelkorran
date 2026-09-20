"""Normal campaign with passive Tarrik blade sampling in freshly earned E4A."""
import json
import os
from pathlib import Path
import runpy
import sys
import time
import traceback
import unreal

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here))
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
os.environ['SOV_AURELION_ROUTE_STOP_AFTER'] = 'continue_aurelion_e4a_input'
started = time.monotonic()
report = dict(status='waiting_for_fresh_e4a', read_only=True)

def watch(delta):
    try:
        if time.monotonic() - started > 1500:
            raise AssertionError('Fresh E4A was not reached within observation bound')
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(pawn, unreal.SovSeleneCharacter) or not pawn.is_character_ready():
            return
        directors = [d for d in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector)
            if str(d.encounter_id) == 'M12_E4_QuarantineCrucibleA']
        if len(directors) != 1 or directors[0].get_encounter_state() != unreal.SovEncounterState.ACTIVE:
            return
        import observe_companion_after_player_shot as contact
        contact.start(out, passive=True)
        report.update(status='observing_fresh_e4a', elapsed=time.monotonic()-started,
            attempt=directors[0].get_attempt_id().export_text())
        unreal.unregister_slate_post_tick_callback(handle)
    except Exception:
        report.update(status='failed', reason=traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
    (out / 'fresh-tarrik-contact.json').write_text(json.dumps(report, indent=2))

handle = unreal.register_slate_post_tick_callback(watch)
(out / 'fresh-tarrik-contact.json').write_text(json.dumps(report, indent=2))
runpy.run_path(str(here / 'start_companion_mesh_route.py'))
