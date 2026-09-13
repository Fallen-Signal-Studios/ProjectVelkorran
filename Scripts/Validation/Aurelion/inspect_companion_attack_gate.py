"""Read the retained companion, target tags and leader damage attribution."""
import json
import os
from pathlib import Path
import unreal

def ref(obj):
    return obj.get_path_name() if obj else None

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc = unreal.GameplayStatics.get_player_controller(world, 0)
rows = []
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovProtagonistCompanionCharacter):
    if actor.get_editor_property('hidden'):
        continue
    controller = actor.get_controller()
    focus = controller.get_focus_actor() if controller else None
    activities = actor.get_activity_component()
    rows.append(dict(actor=ref(actor), focus=ref(focus), weapon=ref(actor.get_weapon()),
        tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(actor).export_text(),
        focus_tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(focus).export_text() if focus else None,
        activity=ref(activities.get_current_activity()), goal=ref(activities.get_current_activity_goal()),
        position=actor.get_actor_location().export_text(),
        focus_position=focus.get_actor_location().export_text() if focus else None))
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'companion-attack-gate.json'
out.write_text(json.dumps(dict(player=ref(pc.get_controlled_pawn()), companions=rows), indent=2), encoding='utf-8')
