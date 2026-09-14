"""Capture facing and locomotion configuration without changing any actor."""
import json
import os
from pathlib import Path
import time
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
rows = []
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovProtagonistCompanionCharacter):
    if actor.get_editor_property('hidden') or not actor.is_alive():
        continue
    controller = actor.get_controller()
    focus = controller.get_focus_actor() if controller else None
    move = actor.get_component_by_class(unreal.CharacterMovementComponent)
    rows.append(dict(actor=actor.get_path_name(), location=actor.get_actor_location().export_text(),
        rotation=actor.get_actor_rotation().export_text(),
        control_rotation=controller.get_control_rotation().export_text() if controller else None,
        focus=focus.get_path_name() if focus else None,
        focus_location=focus.get_actor_location().export_text() if focus else None,
        orient_to_movement=move.get_editor_property('orient_rotation_to_movement'),
        controller_desired_rotation=move.get_editor_property('use_controller_desired_rotation'),
        controller_yaw=actor.get_editor_property('use_controller_rotation_yaw'),
        rotation_rate=move.get_editor_property('rotation_rate').export_text()))
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / ('companion-facing-'+str(time.time_ns())+'.json')
out.write_text(json.dumps(dict(read_only=True, companions=rows), indent=2), encoding='utf-8')
