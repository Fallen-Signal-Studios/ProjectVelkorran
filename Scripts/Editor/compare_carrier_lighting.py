"""Unsaved comparison of local carrier fill; global lighting stays untouched."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(sub.get_all_level_actors());assert len(original)==3140
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
fixtures=[]
specs=[dict(location=[6000,2500,2500],target=[11000,1000,0],width=4000,height=3000,radius=11000,weight=1),
       dict(location=[14000,9500,2000],target=[11000,1000,0],width=4000,height=3000,radius=11000,weight=.4)]
for index,spec in enumerate(specs):
    a=sub.spawn_actor_from_class(unreal.RectLight,unreal.Vector(*spec['location']),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*spec['location']),unreal.Vector(*spec['target'])))
    a.set_actor_label('TEMP_CarrierFill_'+str(index));c=a.get_component_by_class(unreal.RectLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    c.set_source_width(spec['width']);c.set_source_height(spec['height']);c.set_attenuation_radius(spec['radius'])
    c.set_cast_shadows(True);c.set_volumetric_scattering_intensity(0);c.set_light_color(unreal.LinearColor(.72,.82,1,1),False);c.set_intensity(0)
    fixtures.append((a,c,spec))
visuals=[]
for a in original:
    if a.get_actor_label().startswith('Aurelion_Carrier_'):
        c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);visuals.append(c)
        c.set_visibility('_Stable' in a.get_actor_label())
def lighting(level):
    for a,c,spec in fixtures:c.set_intensity(level*spec['weight'])
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[(mode+'-'+name,pos,rot,fov) for mode in ('baseline','soft','balanced') for name,pos,rot,fov in [('route',(6000,-2500,350),(0,30),90),('forward',(6500,10000,2000),(-12,-55),75)]]\n"
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            lighting({'baseline':0,'soft':100000,'balanced':500000}[name.split('-')[0]])")
suffix=suffix.replace('assert len(subsystem.get_all_level_actors())==capture_actor_count',
    "assert len(subsystem.get_all_level_actors())==capture_actor_count\n            for a,c,spec in fixtures: assert subsystem.destroy_actor(a)\n            for c in visuals: c.set_visibility(True)\n            assert len(subsystem.get_all_level_actors())==3140\n            (out/'carrier-lighting-comparison.json').write_text(json.dumps(dict(status='compared_unsaved',fixtures=specs,levels_lumens=[0,100000,500000],restored=True),indent=2))")
exec(compile(prefix+views+'state=dict'+suffix,'carrier_light_comparison','exec'),globals())
