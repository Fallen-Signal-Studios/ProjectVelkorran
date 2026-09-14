"""Fresh-load regression after editor ticks have initialized retained collision."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir())
def verify():
    exec(compile((root/'Scripts/Editor/verify_atrium_rings.py').read_text(),'verify_atrium_rings','exec'),globals())
    assert len(actors)==2472+atrium_canopy_count+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count and atrium_parapet_count==266
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_parapets.py'))['check_parapets'](world,actors)
    fit=json.loads((root/'Art/Source/Aurelion/AtriumParapetKit/guard-fit.json').read_text())
    expected=json.loads((root/'Art/Source/Aurelion/AtriumParapetKit/ring-railing-retained.json').read_text())
    rail=next(c for c in labels[fit['railing_actor']].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==fit['railing_component'])
    assert sorted(rail.get_instance_transform(i,world_space=True).export_text() for i in range(rail.get_instance_count()))==expected['transforms'] and rail.get_instance_count()==116
    assert geometry['passage_controls']==json.loads((root/'Art/Source/Aurelion/AtriumParapetKit/passage-baseline.json').read_text())
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'atrium-parapet-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),retained_railing_instances=116,geometry=geometry),indent=2))
# The soffit diagnostic showed misses before the initial world ticks and exact
# baseline hits after 15 seconds. Keep every assertion; defer only test setup.
if not globals().get('DEFER_ARCHITECTURE_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
