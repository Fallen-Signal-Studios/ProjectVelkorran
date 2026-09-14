"""Fresh-load full architecture regression plus selective railing preservation."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z02_facade_lighting.py').read_text(encoding='utf-8-sig'),'verify_z02_facade_lighting','exec'))
assert len(actors)==2104+north_bridge_count+atrium_guard_count+atrium_ring_count+atrium_parapet_count+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count+z06_gate_assembly_count+z06_flank_landing_count+z07_paving_count+z07_wall_count+z07_light_count and north_guard_count==30
geometry=runpy.run_path(str(root/'Scripts/Editor/check_north_guards.py'))['check_guards'](world,actors)
fit=json.loads((root/'Art/Source/Aurelion/ParapetClosingKit/north-guard-fit.json').read_text())
expected=json.loads((root/'Art/Source/Aurelion/ParapetClosingKit/north-railing-retained.json').read_text())
rail=next(c for c in labels[fit['railing_actor']].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==fit['railing_component'])
assert rail.static_mesh.get_path_name()==fit['railing_mesh']
assert rail.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
actual=sorted(rail.get_instance_transform(i,world_space=True).export_text() for i in range(rail.get_instance_count()))
remaining=expected['transforms'][:]
if atrium_guard_count:
    approved=json.loads((root/'Art/Source/Aurelion/AtriumApproachKit/guard-fit.json').read_text())['removed_instances']
    assert len(approved)==48 and len(remaining)==728
    for item in approved:remaining.remove(item['transform'])
if atrium_parapet_count:
    approved=json.loads((root/'Art/Source/Aurelion/AtriumParapetKit/guard-fit.json').read_text())['removed_instances']
    assert len(approved)==564 and len(remaining)==680
    for item in approved:remaining.remove(item['transform'])
assert actual==remaining and len(actual)==expected['after_count']-(48 if atrium_guard_count else 0)-(564 if atrium_parapet_count else 0)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'north-guard-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),retained_railing_instances=len(actual),geometry=geometry),indent=2))
