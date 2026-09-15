"""Unsaved controlled comparison of the west refuge key light."""
from pathlib import Path
import unreal,json,os
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());key_actor=next(a for a in actors if a.get_actor_label()=='ENVL_Z08_Key_02');key=key_actor.get_component_by_class(unreal.RectLightComponent)
original_intensity=key.intensity;original_radius=key.attenuation_radius;assert original_intensity==1800 and original_radius==9700
settings={'baseline':(1800,9700),'moderate':(700,5850),'balanced':(245,5850)}
code=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views=[(camera+'-'+mode,pos,rot,fov) for camera,pos,rot,fov in [('baffle',(-2600,21680,-980),(-3,90),100),('west-approach',(-3300,21350,-1035),(0,60),90)] for mode in settings]
def set_comparison(name):
    intensity,radius=settings[name.rsplit('-',1)[-1]];key.set_intensity(intensity);key.set_attenuation_radius(radius)
def restore_key():
    key.set_intensity(original_intensity);key.set_attenuation_radius(original_radius)
    (out/'key-comparison.json').write_text(json.dumps(dict(status='unsaved_comparison',settings=settings,restored_intensity=key.intensity,restored_radius=key.attenuation_radius,qualification='Fixed-camera candidates only; no map saved.'),indent=2))
code=prefix+'views='+repr(views)+'\nstate=dict'+suffix
code=code.replace("name,pos,rot,fov=views[state['index']]","name,pos,rot,fov=views[state['index']]\n        set_comparison(name)")
code=code.replace("str(capture_original_delay))","str(capture_original_delay))\n    restore_key()")
exec(compile(code,'refuge_key_comparison','exec'),globals())
