"""Read-only weapon-channel collision and mesh-frame census near the player."""
import json
import os
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
channel = unreal.ArsenalStatics.get_narrative_pro_settings().weapon_trace_channel
rows = []
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NarrativeCharacter):
    if actor.get_distance_to(pawn) > 3000. or not actor.is_alive() or actor.get_editor_property('hidden'):
        continue
    row = dict(actor=actor.get_path_name(), components=[])
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        data = dict(name=component.get_name(), profile=str(component.get_collision_profile_name()),
                    enabled=str(component.get_collision_enabled()), response=str(component.get_collision_response_to_channel(channel)),
                    transform=component.get_world_transform().export_text())
        if isinstance(component, unreal.SkeletalMeshComponent):
            mesh = component.get_skeletal_mesh_asset()
            physics = mesh.get_editor_property('physics_asset') if mesh else None
            data.update(mesh=mesh.get_path_name() if mesh else None,
                        physics=physics.get_path_name() if physics else None,
                        relative=component.get_relative_transform().export_text())
        row['components'].append(data)
    rows.append(row)
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'live-companion-collision.json'
assert not out.exists()
out.write_text(json.dumps(rows, indent=2))
