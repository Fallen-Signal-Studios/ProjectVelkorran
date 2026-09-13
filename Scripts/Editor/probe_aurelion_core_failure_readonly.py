"""Preserve actual elite/Core state after a route failure; never changes gameplay."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'core-failure-readonly.json'
assert not out.exists(), 'Preserve earlier evidence'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
rows = []
for elite in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionElite):
    core = elite.get_core_weak_points()
    thermal = elite.get_thermal_fracture()
    director = thermal.encounter_director
    rows.append(dict(actor=elite.get_path_name(), health=elite.get_health(),
        hidden=elite.get_editor_property('hidden'),
        state=core.capture_weak_point_state().export_text(),
        zones=[z.export_text() for z in core.get_editor_property('weak_point_zones')],
        revealed=core.is_weak_point_reveal_active(),
        receipt=thermal.get_fracture_receipt().export_text(),
        completed=thermal.has_completed_fracture(director, director.get_attempt_id()) if director else None,
        thermal_error=str(thermal.last_error),
        director=director.get_path_name() if director else None))
out.write_text(json.dumps(dict(read_only=True, elites=rows), indent=2), encoding='utf-8')
unreal.log('CORE_FAILURE_READONLY_SAVED: ' + str(out))
