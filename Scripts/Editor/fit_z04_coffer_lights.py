"""Add an upward ceiling wash while preserving existing downlights."""
import unreal

def fit_z04_coffer_lights(actors):
    labels={a.get_actor_label():a for a in actors};sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
    for index,(x,y) in enumerate(((5000,-11900),(9000,-11900),(5000,-10100),(9000,-10100)),1):
        a=labels['ENVL_Z04_CeilingBounce_'+str(index)]
        assert len(a.get_components_by_class(unreal.LightComponent))==1
        a.modify();handles=sub.k2_gather_subobject_data_for_instance(a);parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.RectLightComponent,conform_transform_to_parent=True));assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,'CofferWash');c=lib.get_associated_object(lib.get_data(h));c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_world_transform(unreal.Transform(location=unreal.Vector(x,y,350),rotation=unreal.Rotator(pitch=90),scale=unreal.Vector(1,1,1)),False,False)
        c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(500);c.set_attenuation_radius(3500);c.set_source_width(800);c.set_source_height(800);c.set_cast_shadows(True);c.set_volumetric_scattering_intensity(0)
        c.set_light_color(unreal.LinearColor(.8879231,.9130986,1,1),False)
    a=labels['ENVL_Z04_Key_01'];assert len(a.get_components_by_class(unreal.LightComponent))==1
    a.modify();handles=sub.k2_gather_subobject_data_for_instance(a);parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
    for index,y in enumerate((-11900,-10100),1):
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.RectLightComponent,conform_transform_to_parent=True));assert lib.is_handle_valid(h),reason
        sub.rename_subobject(h,'CenterWash'+str(index));c=lib.get_associated_object(lib.get_data(h));c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_world_transform(unreal.Transform(location=unreal.Vector(7000,y,350),rotation=unreal.Rotator(pitch=90),scale=unreal.Vector(1,1,1)),False,False)
        c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(250);c.set_attenuation_radius(3500);c.set_source_width(800);c.set_source_height(800);c.set_cast_shadows(True);c.set_volumetric_scattering_intensity(0)
        c.set_light_color(unreal.LinearColor(.8879231,.9130986,1,1),False)
