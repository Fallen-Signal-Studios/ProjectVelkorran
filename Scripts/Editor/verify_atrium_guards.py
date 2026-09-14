"""Fresh-load complete architecture chain plus zigzag approach geometry and instance preservation."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_atrium_bridge_exit.py').read_text(),'verify_atrium_bridge_exit','exec'))
assert len(actors)==2142+atrium_ring_count+atrium_parapet_count+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count+z06_gate_assembly_count+z06_flank_landing_count+z07_paving_count+z07_wall_count and atrium_guard_count==34
geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_guards.py'))['check_guards'](world,actors)
fit=json.loads((root/'Art/Source/Aurelion/AtriumApproachKit/guard-fit.json').read_text())
expected=json.loads((root/'Art/Source/Aurelion/AtriumApproachKit/atrium-railing-retained.json').read_text())
rail=next(c for c in labels[fit['railing_actor']].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==fit['railing_component'])
actual=sorted(rail.get_instance_transform(i,world_space=True).export_text() for i in range(rail.get_instance_count()))
remaining=expected['transforms'][:]
if atrium_parapet_count:
    approved=json.loads((root/'Art/Source/Aurelion/AtriumParapetKit/guard-fit.json').read_text())['removed_instances']
    assert len(approved)==564 and len(remaining)==680
    for item in approved:remaining.remove(item['transform'])
assert actual==remaining and len(actual)==680-(564 if atrium_parapet_count else 0)
assert rail.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'atrium-guard-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),retained_railing_instances=len(actual),geometry=geometry),indent=2))
