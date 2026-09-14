"""Fresh saved local light colors and the complete preceding architecture chain."""
from pathlib import Path
import time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_FLANK_LANDING_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_flank_landing.py').read_text(),'verify_z06_flank_landing','exec'),globals())
verify_flank_landing_stage=verify
del DEFER_Z06_FLANK_LANDING_AUTORUN
def verify():
    verify_flank_landing_stage()
    fit=json.loads((root/'Art/Source/Aurelion/Z06LightBalanceReview/key-color-fit.json').read_text());rows=[]
    for row in fit['lights']:
        c=labels[row['actor']].get_component_by_class(unreal.RectLightComponent);color=c.get_editor_property('light_color')
        assert [color.r,color.g,color.b,color.a]==row['after_rgb8']
        assert c.get_world_transform().export_text()==row['transform']
        assert {key:str(c.get_editor_property(key)) for key in row['properties']}==row['properties']
        assert c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
        rows.append(dict(actor=row['actor'],rgb8=row['after_rgb8']))
    assert unreal.SystemLibrary.get_console_variable_int_value('r.ShadowQuality')==5
    assert abs(unreal.SystemLibrary.get_console_variable_float_value('r.Nanite.MaxPixelsPerEdge')-1)<.0001
    assert len(rows)==2 and len(actors)==2843 and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-key-balance-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),lights=rows,qualification='Saved light colors and prior architecture; final visual and live gameplay acceptance remain open.'),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
