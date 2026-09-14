"""Read-only census after an actual exhausted-ammunition route failure."""
import json
import os
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
pc=unreal.GameplayStatics.get_player_controller(world,0)
pawn=pc.get_controlled_pawn(); assert pawn
position=pawn.get_actor_location()
rows=[]
for pickup in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAmmoCombatSustainPickup):
    p=pickup.get_actor_location()
    path=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,position,p,pawn,None)
    rows.append(dict(actor=pickup.get_path_name(),position=p.export_text(),claimed=pickup.is_claimed(),
        hidden=pickup.get_editor_property('hidden'),quantity=pickup.get_ammo_quantity(),
        ammo=pickup.get_ammo_item_class().get_path_name() if pickup.get_ammo_item_class() else None,
        distance=(p-position).length(),complete_path=bool(path and path.is_valid() and not path.is_partial())))
report=dict(world=world.get_path_name(),player=position.export_text(),pickups=rows,
    scope='Read-only live ammo census; no collection, movement, damage or resource changes')
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'exhausted-ammo-census.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('EXHAUSTED_AMMO_CENSUS_COMPLETE '+str(len(rows)))
