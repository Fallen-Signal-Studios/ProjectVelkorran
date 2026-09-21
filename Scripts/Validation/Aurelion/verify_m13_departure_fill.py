"""Read-only CP9 reload verification of saved departure fill in portrait and player view."""
from pathlib import Path
import runpy,time,unreal
root=Path(unreal.Paths.project_dir())
lighting=runpy.run_path(str(root/'Scripts/Editor/aurelion_departure_fill.py'))
def hook(world,state,report,capture,end):
    step=state.setdefault('saved_fill_step',0)
    if time.monotonic()-state['at']<15:return
    if step==0:
        lights=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RectLight)
                if a.get_actor_label().startswith(lighting['PREFIX'])]
        assert len(lights)==2,'Saved departure fills missing'
        specs={lighting['PREFIX']+n:(p,y) for n,p,y in lighting['SPECS']}
        for light in lights:
            position,yaw=specs[light.get_actor_label()];c=light.get_component_by_class(unreal.RectLightComponent)
            assert (light.get_actor_location()-unreal.Vector(*position)).length()<.01
            assert abs(light.get_actor_rotation().pitch+15)<.01 and abs(light.get_actor_rotation().yaw-yaw)<.01
            assert abs(c.intensity-1600)<.01 and abs(c.attenuation_radius-3000)<.01
            assert abs(c.source_width-1400)<.01 and abs(c.source_height-200)<.01
            assert c.get_editor_property('cast_shadows') and abs(c.get_editor_property('specular_scale')-.3)<.001
        report['saved_lights']=[lighting['describe'](a) for a in lights]
        capture(world,'saved-portrait');state.update(saved_fill_step=1,at=time.monotonic());return
    if step==1:
        unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(state['original_view_target'],0)
        state.update(saved_fill_step=2,at=time.monotonic());return
    if step==2:
        capture(world,'saved-player');state.update(saved_fill_step=3,at=time.monotonic());return
    end()
runpy.run_path(str(Path(__file__).with_name('review_selene_face_streaming.py')),
    init_globals={'FACE_REVIEW_SCOPE':__doc__,'FACE_REVIEW_HOOK':hook})
