"""Controlled editor collision reproduction at recorded M13 rider positions.

Temporary characters are destroyed, and no map or asset is saved. This inspects
Pawn-profile clearance, not a mission replay or native transit cancellation.
"""
import hashlib
import json
import os
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
map_file = root / 'Content/Aurelion/Maps/L_Aurelion_M13.umap'
before = hashlib.sha256(map_file.read_bytes()).hexdigest()
editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
report = dict(status='running', qualification=__doc__, samples=[])
spawned = []

def hit_data(value):
    if value is None:
        return dict(blocking=False)
    hits = [v for v in (value if isinstance(value, tuple) else (value,)) if isinstance(v, unreal.HitResult)]
    assert len(hits) == 1
    parts = hits[0].to_tuple()
    return dict(blocking=bool(parts[0]), initial_overlap=bool(parts[1]),
                actor=parts[9].get_path_name() if parts[9] else None,
                component=parts[10].get_path_name() if parts[10] else None,
                raw=hits[0].export_text())

try:
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world.get_name() == 'L_Aurelion_M13'
    lifts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovWorldTransitActor)
    lifts = [a for a in lifts if a.kind == unreal.SovWorldTransitKind.LIFT]
    assert len(lifts) == 1
    specs = [('/Game/PlayerCharacters/BP_SovSelene.BP_SovSelene_C', (110.072809,39033.096186,-1709.85)),
             ('/Game/Aurelion/Characters/BP_AurelionTarrikCompanion.BP_AurelionTarrikCompanion_C', (87.06,38983.291779,-1709.999998))]
    for path, pos in specs:
        actor = editor.spawn_actor_from_class(unreal.load_class(None, path), unreal.Vector(*pos), transient=True)
        assert actor
        spawned.append(actor)
    report['capsules'] = []
    for actor in spawned:
        cap = actor.get_editor_property('capsule_component')
        report['capsules'].append(dict(actor=actor.get_path_name(), position=actor.get_actor_location().export_text(),
            radius=cap.get_scaled_capsule_radius(), half_height=cap.get_scaled_capsule_half_height(),
            profile=str(cap.get_collision_profile_name()), pawn_response=str(cap.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN))))
        start = actor.get_actor_location()
        for ignore_corider in (False, True):
            for rise in (0.1, 1., 5.):
                ignored = list(lifts) + (list(spawned) if ignore_corider else [actor])
                result = unreal.SystemLibrary.capsule_trace_single_by_profile(world, start,
                    unreal.Vector(start.x, start.y, start.z + rise), cap.get_scaled_capsule_radius()-2.,
                    cap.get_scaled_capsule_half_height()-2., 'Pawn', False, ignored, unreal.DrawDebugTrace.NONE, True)
                report['samples'].append(dict(actor=actor.get_path_name(), ignore_corider=ignore_corider,
                    rise=rise, hit=hit_data(result)))
    report['status'] = 'observed_requires_interpretation'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
finally:
    for actor in reversed(spawned):
        editor.destroy_actor(actor)
    report['map_unchanged'] = before == hashlib.sha256(map_file.read_bytes()).hexdigest()
    (out / 'lift-corider-clearance.json').write_text(json.dumps(report, indent=2))
