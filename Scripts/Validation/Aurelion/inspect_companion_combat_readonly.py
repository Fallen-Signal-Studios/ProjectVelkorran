"""Read live protagonist proxies and their authored combat setup; never change gameplay."""
import json
from pathlib import Path
import unreal


def ref(obj):
    return obj.get_path_name() if obj else None


def read(obj, method):
    try:
        value = getattr(obj, method)()
        return ref(value) if isinstance(value, unreal.Object) else str(value)
    except Exception as error:
        return {"unavailable": str(error)}


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "Requires retained PIE"
rows = []
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovProtagonistCompanionCharacter):
    row = {"actor": ref(actor), "location": str(actor.get_actor_location()),
           "identity": str(actor.get_companion_identity()), "controller": ref(actor.get_controller()),
           "state": str(actor.get_companion_component().get_command_state())}
    for method in ("get_weapon", "get_inventory_component", "get_character_visual", "get_activity_component", "get_narrative_ability_system_component"):
        row[method] = read(actor, method)
    row['inventory_items'] = [ref(item) for item in actor.get_inventory_component().get_items()]
    row['activity'] = read(actor.get_activity_component(), 'get_current_activity')
    row['goal'] = read(actor.get_activity_component(), 'get_current_activity_goal')
    rows.append(row)
definitions = {}
for hero in ("Selene", "Tarrik"):
    npc = unreal.load_asset('/Game/Aurelion/Characters/NPC_Aurelion' + hero + 'Companion')
    definitions[hero] = {field: str(npc.get_editor_property(field)) for field in (
        'default_item_loadout', 'activity_configuration', 'ability_configuration', 'default_appearance')}
target = Path(__file__).resolve().parents[3] / 'Saved/Validation/Aurelion/CompanionCombat-20260913/live.json'
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(json.dumps({"proxies": rows, "definitions": definitions}, indent=2), encoding='utf-8')
unreal.log('Companion combat read-only report: ' + str(target))

