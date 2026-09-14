"""Read-only lights affecting Z06 and post-process settings for the next art pass."""
from pathlib import Path
import json,os
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
def properties(obj,names):
    result={}
    for name in names:
        try:result[name]=str(obj.get_editor_property(name))
        except Exception:pass
    return result
lights=[];post=[]
for a in actors:
    for c in a.get_components_by_class(unreal.LightComponentBase):
        p=c.get_world_location()
        try:radius=float(c.get_editor_property('attenuation_radius'))
        except Exception:radius=None
        if radius is not None:
            distance=sum(max(low-value,0,value-high)**2 for value,low,high in zip((p.x,p.y,p.z),(-1700,6000,-600),(1700,11600,100)))**.5
            if distance>radius:continue
        lights.append(dict(actor=a.get_actor_label(),component=c.get_name(),class_name=c.get_class().get_name(),transform=c.get_world_transform().export_text(),properties=properties(c,('intensity','intensity_units','light_color','temperature','use_temperature','attenuation_radius','source_radius','source_width','source_height','cast_shadows','cast_contact_shadow','contact_shadow_length','indirect_lighting_intensity','volumetric_scattering_intensity','mobility','visible'))))
    objects=[a] if isinstance(a,unreal.PostProcessVolume) else []
    objects+=list(a.get_components_by_class(unreal.PostProcessComponent))
    for obj in objects:
        try:settings=obj.get_editor_property('settings')
        except Exception:continue
        names=('white_temp','white_tint','auto_exposure_bias','auto_exposure_min_brightness','auto_exposure_max_brightness','color_saturation','color_contrast','color_gamma','color_gain','scene_color_tint','bloom_intensity','vignette_intensity')
        post.append(dict(actor=a.get_actor_label(),object=obj.get_name(),properties=properties(obj,('enabled','unbound','priority','blend_weight','blend_radius')),settings=properties(settings,names+tuple('override_'+n for n in names))))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z06-light-balance-audit.json').write_text(json.dumps(dict(status='read_only',lights=lights,post_process=post,qualification='Potential influence by range; renderer contribution and visual cause require controlled comparisons.'),indent=2))
