"""Unsaved baseline/300/900 lumen ceiling-fill comparison, preserving scanner lights."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors};sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary;fills=[]
for i in range(3):
    a=labels['ENVL_Z03_Key_'+str(i).zfill(2)]
    assert not any(c.get_name()=='CofferWash' for c in a.get_components_by_class(unreal.RectLightComponent))
    handles=sub.k2_gather_subobject_data_for_instance(a);parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
    h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.RectLightComponent,conform_transform_to_parent=True));assert lib.is_handle_valid(h),reason
    sub.rename_subobject(h,'CofferWash');c=lib.get_associated_object(lib.get_data(h));c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_world_transform(unreal.Transform(location=a.get_actor_location(),rotation=unreal.Rotator(pitch=90),scale=unreal.Vector(1,1,1)),False,False)
    c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(0);c.set_attenuation_radius(2500);c.set_source_width(240);c.set_source_height(160);c.set_cast_shadows(True);c.set_volumetric_scattering_intensity(0);c.set_light_color(unreal.LinearColor(.8879231,.9130986,1,1),False);fills.append(c)
    if globals().get('Z03_WIDE_FILL',False):
        c.set_world_location(unreal.Vector(7000,a.get_actor_location().y,300),False,False);c.set_source_width(600);c.set_source_height(600)
code=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
suffix=suffix.replace("camera.set_actor_location(unreal.Vector(*pos),False,False)","[c.set_intensity(float(name.rsplit('-',1)[1])) for c in fills];camera.set_actor_location(unreal.Vector(*pos),False,False)")
views="views=[(name+'-'+str(power),pos,rot,90) for name,pos,rot in [('approach',(6800,-19300,165),(0,90)),('middle',(6800,-17200,165),(15,90))] for power in (0,300,900)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z03_fill_comparison','exec'),globals())
